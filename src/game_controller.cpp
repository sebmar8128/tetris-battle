#include "game_controller.h"

#include <string.h>

#include "default_settings.h"
#include "queues.h"

namespace {

constexpr char UNKNOWN_REMOTE_USERNAME[] = "REMOTE";
constexpr uint8_t MAX_STARTING_LEVEL = 29;
constexpr uint8_t USER_SETTINGS_ITEM_COUNT = 7;
constexpr uint8_t USERNAME_ENTRY_ITEM_COUNT = 3;
constexpr char USERNAME_BLANK = ' ';

PresenceState presenceForPhase(GamePhase phase) {
    switch (phase) {
        case GamePhase::Welcome:
        case GamePhase::UsernameEntry:
        case GamePhase::UserSettings:
            return PresenceState::NotInLobby;
        case GamePhase::Lobby: return PresenceState::InLobby;
        case GamePhase::Gameplay: return PresenceState::Gameplay;
        case GamePhase::Paused: return PresenceState::Paused;
        case GamePhase::GameOver: return PresenceState::GameOver;
    }

    return PresenceState::NotInLobby;
}

LobbyPeerStatus lobbyPeerStatus(bool remoteOnline, PresenceState remotePresenceState) {
    if (!remoteOnline) {
        return LobbyPeerStatus::Offline;
    }

    return remotePresenceState == PresenceState::InLobby
        ? LobbyPeerStatus::InLobby
        : LobbyPeerStatus::Online;
}

bool isMenuLeft(PhysicalButton button) {
    return button == PhysicalButton::LdLeft || button == PhysicalButton::RdLeft;
}

bool isMenuRight(PhysicalButton button) {
    return button == PhysicalButton::LdRight || button == PhysicalButton::RdRight;
}

bool isMenuUp(PhysicalButton button) {
    return button == PhysicalButton::LdUp || button == PhysicalButton::RdUp;
}

bool isMenuDown(PhysicalButton button) {
    return button == PhysicalButton::LdDown || button == PhysicalButton::RdDown;
}

bool userSettingsEqual(const UserSettings& a, const UserSettings& b) {
    return strncmp(a.username, b.username, MAX_USERNAME_LEN + 1) == 0 &&
           a.theme == b.theme &&
           a.holdEnabled == b.holdEnabled &&
           a.ghostEnabled == b.ghostEnabled &&
           a.nextPreviewCount == b.nextPreviewCount;
}

uint8_t usernameLength(const char* username) {
    uint8_t length = 0;
    while (length < MAX_USERNAME_LEN && username[length] != '\0') {
        ++length;
    }
    return length;
}

bool usernameIsValid(const char* username) {
    const uint8_t length = usernameLength(username);
    if (length == 0) {
        return false;
    }

    for (uint8_t i = 0; i < length; ++i) {
        if (username[i] < 'A' || username[i] > 'Z') {
            return false;
        }
    }

    return true;
}

template <typename EnumType>
EnumType nextEnumValue(EnumType current, int8_t delta, uint8_t count) {
    int16_t value = static_cast<int16_t>(current) + delta;
    while (value < 0) {
        value += count;
    }
    while (value >= count) {
        value -= count;
    }
    return static_cast<EnumType>(value);
}

}  // namespace

void GameController::begin() {
    userSettings_ = DefaultSettings::userSettings();
    draftUserSettings_ = userSettings_;
    matchSettings_ = DefaultSettings::matchSettings();

    phase_ = GamePhase::Welcome;
    localDeviceId_ = 0;
    transportReady_ = false;

    welcomeSelection_ = WelcomeMenuItem::SignIn;
    usernameEntryMode_ = UsernameEntryMode::SignIn;
    usernameEntrySelection_ = UsernameEntryItem::Letters;
    usernameEntryMessage_ = UsernameEntryMessage::None;
    strncpy(usernameDraft_, "A", MAX_USERNAME_LEN);
    usernameDraft_[1] = '\0';
    usernameCursorIndex_ = 0;
    usernameSlotActive_ = false;
    storageBusy_ = false;
    displayConfigDirty_ = true;
    pendingStorageOperation_ = StorageOperation::Load;
    pendingStorageMode_ = UsernameEntryMode::SignIn;
    pendingSaveSettings_ = {};
    userSettingsSelection_ = UserSettingsMenuItem::Theme;
    pendingUserSettingsExit_ = UserSettingsExitAction::None;
    userSettingsConfirmContinueSelected_ = false;

    remoteOnline_ = false;
    remoteDeviceId_ = 0;
    remotePresenceState_ = PresenceState::NotInLobby;
    strncpy(remoteUsername_, UNKNOWN_REMOTE_USERNAME, MAX_USERNAME_LEN);
    remoteUsername_[MAX_USERNAME_LEN] = '\0';

    settingsRevision_ = 0;
    settingsOwnerId_ = 0;
    currentSeed_ = DefaultSettings::GAME_SEED;

    lobbySelection_ = LobbyMenuItem::GarbageEnabled;
    lobbyModalState_ = ModalState::None;
    lobbyConfirmAcceptSelected_ = true;
    pendingIncomingStart_ = {};
    pendingOutgoingStart_ = {};
    nextStartRequestId_ = 1;

    currentPauseId_ = 0;
    pauseSelection_ = PauseMenuAction::Resume;
    pausePanelState_ = PausePanelState::Menu;
    pauseConfirmAcceptSelected_ = true;
    pendingIncomingPauseAction_ = {};
    pendingOutgoingPauseAction_ = {};
    nextPauseId_ = 1;
    nextPauseActionRequestId_ = 1;

    localTerminal_ = false;
    remoteTerminal_ = false;
    localFinishedBeforeRemote_ = false;
    localWon_ = false;
    disconnected_ = false;
    localGameOverReason_ = GameOverReason::TopOut;
    remoteGameOverReason_ = GameOverReason::TopOut;
    remoteFinalScore_ = 0;
    remoteLinesCleared_ = 0;
    postGameSelection_ = PostGameMenuAction::Restart;
    gameOverPanelState_ = GameOverPanelState::WaitingForRemote;
    gameOverConfirmAcceptSelected_ = true;
    pendingIncomingPostGameAction_ = {};
    pendingOutgoingPostGameAction_ = {};
    nextPostGameRequestId_ = 1;
    localGameOverPacketQueued_ = false;

    outboundHead_ = 0;
    outboundCount_ = 0;
    musicHead_ = 0;
    musicCount_ = 0;
}

bool GameController::handleInputEvent(const InputEvent& event) {
    switch (phase_) {
        case GamePhase::Welcome:
            return handleWelcomeInput(event);
        case GamePhase::UsernameEntry:
            return handleUsernameEntryInput(event);
        case GamePhase::UserSettings:
            return handleUserSettingsInput(event);
        case GamePhase::Lobby:
            return handleLobbyInput(event);
        case GamePhase::Gameplay:
            return handleGameplayInput(event);
        case GamePhase::Paused:
            return handlePausedInput(event);
        case GamePhase::GameOver:
            return handleGameOverInput(event);
    }

    return false;
}

bool GameController::handleRemoteEvent(const RemoteEvent& event) {
    switch (event.type) {
        case RemoteEventType::TransportReady:
            localDeviceId_ = event.payload.transportReady.localDeviceId;
            transportReady_ = true;
            return false;
        case RemoteEventType::PacketReceived:
            return handlePacket(event.payload.packet);
        case RemoteEventType::PeerDisconnected:
            remoteOnline_ = false;
            remotePresenceState_ = PresenceState::NotInLobby;
            if ((phase_ == GamePhase::Gameplay || phase_ == GamePhase::Paused) && !localTerminal_) {
                disconnected_ = true;
                finalizeLocalGameOver(GameOverReason::Disconnect);
                return true;
            }
            if (phase_ == GamePhase::GameOver && localTerminal_ && !remoteTerminal_) {
                disconnected_ = true;
                updatePostGameState();
                return true;
            }
            return phase_ == GamePhase::Lobby;
    }

    return false;
}

bool GameController::handleStorageResponse(const StorageResponse& response) {
    if (!storageBusy_ || response.key != StorageKey::UserSettings) {
        return false;
    }

    const StorageOperation operation = pendingStorageOperation_;
    const UsernameEntryMode mode = pendingStorageMode_;
    const UserSettings pendingSaveSettings = pendingSaveSettings_;
    resetStoragePending();

    if (operation == StorageOperation::Load) {
        if (mode == UsernameEntryMode::SignIn) {
            if (response.status == StorageStatus::Success) {
                enterUserSettings(response.payload.userSettings);
            } else {
                usernameEntryMessage_ = response.status == StorageStatus::NotFound
                    ? UsernameEntryMessage::UserNotFound
                    : UsernameEntryMessage::StorageFailed;
            }
            return true;
        }

        if (response.status == StorageStatus::Success) {
            usernameEntryMessage_ = UsernameEntryMessage::UserExists;
            return true;
        }

        if (response.status != StorageStatus::NotFound) {
            usernameEntryMessage_ = UsernameEntryMessage::StorageFailed;
            return true;
        }

        UserSettings newSettings = DefaultSettings::userSettings();
        strncpy(newSettings.username, usernameDraft_, MAX_USERNAME_LEN);
        newSettings.username[MAX_USERNAME_LEN] = '\0';
        queueUserSave(newSettings);
        pendingStorageMode_ = UsernameEntryMode::NewUser;
        return true;
    }

    if (operation == StorageOperation::Save) {
        if (response.status == StorageStatus::Success) {
            userSettings_ = pendingSaveSettings;
            draftUserSettings_ = pendingSaveSettings;
            displayConfigDirty_ = true;
            pendingUserSettingsExit_ = UserSettingsExitAction::None;
            usernameEntryMessage_ = UsernameEntryMessage::None;

            if (phase_ == GamePhase::UsernameEntry) {
                enterUserSettings(userSettings_);
            }
        } else if (phase_ == GamePhase::UsernameEntry) {
            usernameEntryMessage_ = UsernameEntryMessage::StorageFailed;
        }
        return true;
    }

    return false;
}

bool GameController::tick(uint32_t nowMs) {
    if (phase_ != GamePhase::Gameplay || localTerminal_) {
        return false;
    }

    return handleStepResult(engine_.tick(nowMs));
}

bool GameController::popOutboundPacket(NetPacket& packet) {
    if (outboundCount_ == 0) {
        return false;
    }

    packet = outboundPackets_[outboundHead_];
    outboundHead_ = (outboundHead_ + 1) % OUTBOUND_PACKET_CAPACITY;
    --outboundCount_;
    return true;
}

bool GameController::popMusicEvent(MusicEvent& event) {
    if (musicCount_ == 0) {
        return false;
    }

    event = musicEvents_[musicHead_];
    musicHead_ = (musicHead_ + 1) % MUSIC_EVENT_CAPACITY;
    --musicCount_;
    return true;
}

bool GameController::consumeDisplayConfigDirty() {
    if (!displayConfigDirty_) {
        return false;
    }

    displayConfigDirty_ = false;
    return true;
}

void GameController::makeDisplayConfigEvent(RenderEvent& event) const {
    event = {};
    event.type = RenderEventType::Config;
    event.payload.config = {
        userSettings_.theme,
        userSettings_.holdEnabled,
        userSettings_.ghostEnabled,
        userSettings_.nextPreviewCount
    };
}

void GameController::makeScreenRenderEvent(RenderEvent& event) const {
    event = {};
    event.type = RenderEventType::Screen;

    switch (phase_) {
        case GamePhase::Welcome:
            event.payload.screen.screen = RenderScreen::Welcome;
            event.payload.screen.payload.welcome = {
                welcomeSelection_
            };
            return;

        case GamePhase::UsernameEntry:
            event.payload.screen.screen = RenderScreen::UsernameEntry;
            event.payload.screen.payload.usernameEntry = {};
            event.payload.screen.payload.usernameEntry.mode = usernameEntryMode_;
            strncpy(event.payload.screen.payload.usernameEntry.username, usernameDraft_, MAX_USERNAME_LEN);
            event.payload.screen.payload.usernameEntry.username[MAX_USERNAME_LEN] = '\0';
            event.payload.screen.payload.usernameEntry.cursorIndex = usernameCursorIndex_;
            event.payload.screen.payload.usernameEntry.slotActive = usernameSlotActive_;
            event.payload.screen.payload.usernameEntry.selectedItem = usernameEntrySelection_;
            event.payload.screen.payload.usernameEntry.message = usernameEntryMessage_;
            event.payload.screen.payload.usernameEntry.busy = storageBusy_;
            return;

        case GamePhase::UserSettings:
            event.payload.screen.screen = RenderScreen::UserSettings;
            event.payload.screen.payload.userSettings = {};
            strncpy(event.payload.screen.payload.userSettings.username, draftUserSettings_.username, MAX_USERNAME_LEN);
            event.payload.screen.payload.userSettings.username[MAX_USERNAME_LEN] = '\0';
            event.payload.screen.payload.userSettings.settings = draftUserSettings_;
            event.payload.screen.payload.userSettings.selectedItem = userSettingsSelection_;
            event.payload.screen.payload.userSettings.modalState = pendingUserSettingsExit_ == UserSettingsExitAction::None
                ? ModalState::None
                : ModalState::UnsavedSettings;
            event.payload.screen.payload.userSettings.pendingExitAction = pendingUserSettingsExit_;
            event.payload.screen.payload.userSettings.confirmContinueSelected = userSettingsConfirmContinueSelected_;
            event.payload.screen.payload.userSettings.dirty = userSettingsDirty();
            event.payload.screen.payload.userSettings.busy = storageBusy_;
            return;

        case GamePhase::Lobby:
            event.payload.screen.screen = RenderScreen::Lobby;
            strncpy(event.payload.screen.payload.lobby.localUsername, userSettings_.username, MAX_USERNAME_LEN);
            event.payload.screen.payload.lobby.localUsername[MAX_USERNAME_LEN] = '\0';
            strncpy(event.payload.screen.payload.lobby.remoteUsername, remoteUsername_, MAX_USERNAME_LEN);
            event.payload.screen.payload.lobby.remoteUsername[MAX_USERNAME_LEN] = '\0';
            event.payload.screen.payload.lobby.remoteStatus = lobbyPeerStatus(remoteOnline_, remotePresenceState_);
            event.payload.screen.payload.lobby.matchSettings = matchSettings_;
            event.payload.screen.payload.lobby.revision = settingsRevision_;
            event.payload.screen.payload.lobby.selectedItem = lobbySelection_;
            event.payload.screen.payload.lobby.modalState = lobbyModalState_;
            event.payload.screen.payload.lobby.incomingStartRequest = pendingIncomingStart_.valid;
            event.payload.screen.payload.lobby.confirmAcceptSelected = lobbyConfirmAcceptSelected_;
            return;

        case GamePhase::Gameplay:
            event.payload.screen.screen = RenderScreen::Gameplay;
            event.payload.screen.payload.gameplay = {
                engine_.renderState(),
                remoteTerminal_
            };
            return;

        case GamePhase::Paused:
            event.payload.screen.screen = RenderScreen::Paused;
            event.payload.screen.payload.pause = {
                engine_.renderState(),
                pausePanelState_,
                pauseSelection_,
                pendingIncomingPauseAction_.valid
                    ? pendingIncomingPauseAction_.action
                    : (pendingOutgoingPauseAction_.valid
                        ? pendingOutgoingPauseAction_.action
                        : pauseSelection_),
                pauseConfirmAcceptSelected_
            };
            return;

        case GamePhase::GameOver:
            event.payload.screen.screen = RenderScreen::GameOver;
            event.payload.screen.payload.gameOver = {
                engine_.renderState(),
                localGameOverReason_,
                remoteTerminal_,
                localWon_,
                localTerminal_ && remoteTerminal_,
                disconnected_,
                gameOverPanelState_,
                postGameSelection_,
                pendingIncomingPostGameAction_.valid
                    ? pendingIncomingPostGameAction_.action
                    : (pendingOutgoingPostGameAction_.valid
                        ? pendingOutgoingPostGameAction_.action
                        : postGameSelection_),
                gameOverConfirmAcceptSelected_
            };
            return;
    }
}

bool GameController::handleGameplayInput(const InputEvent& event) {
    if (event.button == PhysicalButton::Center) {
        if (event.type == InputEventType::Press) {
            const uint8_t pauseId = nextPauseId_++;
            queuePausePacket(pauseId);
            enterPaused(pauseId);
            return true;
        }
        return false;
    }

    if (event.button == userSettings_.buttonMapping.softDrop) {
        if (event.type == InputEventType::Press) {
            return handleStepResult(engine_.applyAction(GameEngine::Action::SoftDropPressed, event.tickMs));
        }

        if (event.type == InputEventType::Release) {
            return handleStepResult(engine_.applyAction(GameEngine::Action::SoftDropReleased, event.tickMs));
        }

        return false;
    }

    GameEngine::Action action = GameEngine::Action::MoveLeft;

    if (event.type == InputEventType::Press && mapsToAction(event.button, action)) {
        return handleStepResult(engine_.applyAction(action, event.tickMs));
    }

    if (event.type == InputEventType::Repeat && mapsToRepeatAction(event.button, action)) {
        return handleStepResult(engine_.applyAction(action, event.tickMs));
    }

    return false;
}

bool GameController::handleWelcomeInput(const InputEvent& event) {
    if (event.type != InputEventType::Press && event.type != InputEventType::Repeat) {
        return false;
    }

    if (isNavigationEvent(event)) {
        welcomeSelection_ = welcomeSelection_ == WelcomeMenuItem::SignIn
            ? WelcomeMenuItem::NewUser
            : WelcomeMenuItem::SignIn;
        return true;
    }

    if (!isSelectEvent(event)) {
        return false;
    }

    enterUsernameEntry(
        welcomeSelection_ == WelcomeMenuItem::SignIn
            ? UsernameEntryMode::SignIn
            : UsernameEntryMode::NewUser
    );
    return true;
}

bool GameController::handleUsernameEntryInput(const InputEvent& event) {
    if (storageBusy_) {
        return false;
    }

    if (event.type != InputEventType::Press && event.type != InputEventType::Repeat) {
        return false;
    }

    if (usernameSlotActive_) {
        if (isMenuLeft(event.button) || isMenuRight(event.button)) {
            return applyUsernameSlotDelta(isMenuRight(event.button) ? 1 : -1);
        }

        if (isSelectEvent(event)) {
            usernameSlotActive_ = false;
            normalizeUsernameCursorAfterEdit();
            return true;
        }

        return false;
    }

    if (isMenuUp(event.button) || isMenuDown(event.button)) {
        usernameEntrySelection_ = nextEnumValue(
            usernameEntrySelection_,
            isMenuDown(event.button) ? 1 : -1,
            USERNAME_ENTRY_ITEM_COUNT
        );
        usernameEntryMessage_ = UsernameEntryMessage::None;
        return true;
    }

    if (usernameEntrySelection_ == UsernameEntryItem::Letters &&
        (isMenuLeft(event.button) || isMenuRight(event.button))) {
        const int8_t delta = isMenuRight(event.button) ? 1 : -1;
        const uint8_t length = usernameLength(usernameDraft_);
        const uint8_t maxIndex = min<uint8_t>(MAX_USERNAME_LEN - 1, length);
        int16_t index = static_cast<int16_t>(usernameCursorIndex_) + delta;
        if (index < 0) {
            index = maxIndex;
        } else if (index > maxIndex) {
            index = 0;
        }
        usernameCursorIndex_ = static_cast<uint8_t>(index);
        usernameEntryMessage_ = UsernameEntryMessage::None;
        return true;
    }

    if (!isSelectEvent(event)) {
        return false;
    }

    switch (usernameEntrySelection_) {
        case UsernameEntryItem::Letters:
            setUsernameDraftFromCurrentLength();
            usernameSlotActive_ = true;
            usernameEntryMessage_ = UsernameEntryMessage::None;
            return true;

        case UsernameEntryItem::Continue:
            if (!usernameIsValid(usernameDraft_)) {
                return false;
            }
            queueUserLoad(usernameDraft_);
            return true;

        case UsernameEntryItem::Back:
            enterWelcome();
            return true;
    }

    return false;
}

bool GameController::handleUserSettingsInput(const InputEvent& event) {
    if (storageBusy_) {
        return false;
    }

    if (pendingUserSettingsExit_ != UserSettingsExitAction::None) {
        if (isNavigationEvent(event)) {
            userSettingsConfirmContinueSelected_ = !userSettingsConfirmContinueSelected_;
            return true;
        }

        if (!isSelectEvent(event)) {
            return false;
        }

        if (userSettingsConfirmContinueSelected_) {
            const UserSettingsExitAction action = pendingUserSettingsExit_;
            pendingUserSettingsExit_ = UserSettingsExitAction::None;
            if (action == UserSettingsExitAction::JoinLobby) {
                joinLobbyFromSettings();
            } else {
                signOutFromSettings();
            }
        } else {
            pendingUserSettingsExit_ = UserSettingsExitAction::None;
        }
        return true;
    }

    if (event.type != InputEventType::Press && event.type != InputEventType::Repeat) {
        return false;
    }

    if (isMenuUp(event.button) || isMenuDown(event.button)) {
        userSettingsSelection_ = nextEnumValue(
            userSettingsSelection_,
            isMenuDown(event.button) ? 1 : -1,
            USER_SETTINGS_ITEM_COUNT
        );
        return true;
    }

    if (isMenuLeft(event.button) || isMenuRight(event.button)) {
        const int8_t delta = isMenuRight(event.button) ? 1 : -1;
        switch (userSettingsSelection_) {
            case UserSettingsMenuItem::Theme:
                draftUserSettings_.theme = nextEnumValue(draftUserSettings_.theme, delta, 3);
                return true;
            case UserSettingsMenuItem::HoldEnabled:
                draftUserSettings_.holdEnabled = !draftUserSettings_.holdEnabled;
                return true;
            case UserSettingsMenuItem::GhostEnabled:
                draftUserSettings_.ghostEnabled = !draftUserSettings_.ghostEnabled;
                return true;
            case UserSettingsMenuItem::NextPreviewCount:
            {
                int16_t count = static_cast<int16_t>(draftUserSettings_.nextPreviewCount) + delta;
                if (count < 0) {
                    count = MAX_NEXT_PIECES;
                } else if (count > MAX_NEXT_PIECES) {
                    count = 0;
                }
                draftUserSettings_.nextPreviewCount = static_cast<uint8_t>(count);
                return true;
            }
            case UserSettingsMenuItem::SaveSettings:
            case UserSettingsMenuItem::JoinLobby:
            case UserSettingsMenuItem::SignOut:
                return false;
        }
    }

    if (!isSelectEvent(event)) {
        return false;
    }

    switch (userSettingsSelection_) {
        case UserSettingsMenuItem::SaveSettings:
            queueUserSave(draftUserSettings_);
            return true;
        case UserSettingsMenuItem::JoinLobby:
            if (userSettingsDirty()) {
                pendingUserSettingsExit_ = UserSettingsExitAction::JoinLobby;
                userSettingsConfirmContinueSelected_ = false;
            } else {
                joinLobbyFromSettings();
            }
            return true;
        case UserSettingsMenuItem::SignOut:
            if (userSettingsDirty()) {
                pendingUserSettingsExit_ = UserSettingsExitAction::SignOut;
                userSettingsConfirmContinueSelected_ = false;
            } else {
                signOutFromSettings();
            }
            return true;
        case UserSettingsMenuItem::Theme:
        case UserSettingsMenuItem::HoldEnabled:
        case UserSettingsMenuItem::GhostEnabled:
        case UserSettingsMenuItem::NextPreviewCount:
            return false;
    }

    return false;
}

bool GameController::handleLobbyInput(const InputEvent& event) {
    if (lobbyModalState_ == ModalState::OutgoingRequest) {
        return false;
    }

    if (lobbyModalState_ == ModalState::IncomingRequest) {
        if (isNavigationEvent(event)) {
            lobbyConfirmAcceptSelected_ = !lobbyConfirmAcceptSelected_;
            return true;
        }

        if (!isSelectEvent(event) || !pendingIncomingStart_.valid) {
            return false;
        }

        queueStartGameReplyPacket(pendingIncomingStart_.requestId, lobbyConfirmAcceptSelected_);
        if (lobbyConfirmAcceptSelected_) {
            matchSettings_ = pendingIncomingStart_.settings;
            settingsRevision_ = pendingIncomingStart_.settingsRevision;
            settingsOwnerId_ = pendingIncomingStart_.settingsOwnerId;
            beginGame(pendingIncomingStart_.seed, pendingIncomingStart_.settings);
        } else {
            pendingIncomingStart_ = {};
            lobbyModalState_ = ModalState::None;
        }
        return true;
    }

    if (event.type != InputEventType::Press && event.type != InputEventType::Repeat) {
        return false;
    }

    if (isMenuUp(event.button)) {
        lobbySelection_ = nextEnumValue(lobbySelection_, -1, 5);
        return true;
    }

    if (isMenuDown(event.button)) {
        lobbySelection_ = nextEnumValue(lobbySelection_, 1, 5);
        return true;
    }

    if (isMenuLeft(event.button) || isMenuRight(event.button)) {
        const int8_t delta = isMenuRight(event.button) ? 1 : -1;
        bool changed = false;

        switch (lobbySelection_) {
            case LobbyMenuItem::GarbageEnabled:
                matchSettings_.garbageEnabled = !matchSettings_.garbageEnabled;
                changed = true;
                break;
            case LobbyMenuItem::Mode:
                matchSettings_.mode = matchSettings_.mode == GameMode::Marathon
                    ? GameMode::Sprint40
                    : GameMode::Marathon;
                changed = true;
                break;
            case LobbyMenuItem::StartingLevel:
            {
                int16_t level = static_cast<int16_t>(matchSettings_.startingLevel) + delta;
                if (level < 0) {
                    level = MAX_STARTING_LEVEL;
                } else if (level > MAX_STARTING_LEVEL) {
                    level = 0;
                }
                matchSettings_.startingLevel = static_cast<uint8_t>(level);
                changed = true;
                break;
            }
            case LobbyMenuItem::MusicEnabled:
                matchSettings_.musicEnabled = !matchSettings_.musicEnabled;
                changed = true;
                break;
            case LobbyMenuItem::StartGame:
                break;
        }

        if (changed) {
            ++settingsRevision_;
            settingsOwnerId_ = transportReady_ ? localDeviceId_ : 0;
            queueLobbySettingsPacket();
        }
        return changed;
    }

    if (!isSelectEvent(event) || lobbySelection_ != LobbyMenuItem::StartGame) {
        return false;
    }

    if (!remoteOnline_ || remotePresenceState_ != PresenceState::InLobby) {
        return false;
    }

    pendingOutgoingStart_ = {};
    pendingOutgoingStart_.valid = true;
    pendingOutgoingStart_.requestId = nextStartRequestId_++;
    pendingOutgoingStart_.settingsRevision = settingsRevision_;
    pendingOutgoingStart_.settingsOwnerId = settingsOwnerId_;
    pendingOutgoingStart_.settings = matchSettings_;
    pendingOutgoingStart_.seed = (millis() << 4) ^ currentSeed_ ^ localDeviceId_ ^ settingsRevision_;

    lobbyModalState_ = ModalState::OutgoingRequest;
    queueStartGameRequestPacket(pendingOutgoingStart_.seed);
    return true;
}

bool GameController::handlePausedInput(const InputEvent& event) {
    if (pausePanelState_ == PausePanelState::OutgoingRequest) {
        return false;
    }

    if (pausePanelState_ == PausePanelState::IncomingRequest) {
        if (isNavigationEvent(event)) {
            pauseConfirmAcceptSelected_ = !pauseConfirmAcceptSelected_;
            return true;
        }

        if (!isSelectEvent(event) || !pendingIncomingPauseAction_.valid) {
            return false;
        }

        queuePauseActionReplyPacket(pendingIncomingPauseAction_, pauseConfirmAcceptSelected_);
        if (pauseConfirmAcceptSelected_) {
            const PauseActionState accepted = pendingIncomingPauseAction_;
            pendingIncomingPauseAction_ = {};
            executePauseAction(accepted.action, accepted.restartSeed);
        } else {
            pendingIncomingPauseAction_ = {};
            pausePanelState_ = PausePanelState::Menu;
        }
        return true;
    }

    if (event.type != InputEventType::Press && event.type != InputEventType::Repeat) {
        return false;
    }

    if (isNavigationEvent(event)) {
        const int8_t delta = navigationDelta(event);
        pauseSelection_ = nextEnumValue(pauseSelection_, delta, 3);
        return true;
    }

    if (!isSelectEvent(event)) {
        return false;
    }

    pendingOutgoingPauseAction_ = {};
    pendingOutgoingPauseAction_.valid = true;
    pendingOutgoingPauseAction_.pauseId = currentPauseId_;
    pendingOutgoingPauseAction_.requestId = nextPauseActionRequestId_++;
    pendingOutgoingPauseAction_.action = pauseSelection_;
    pendingOutgoingPauseAction_.restartSeed = (millis() << 3) ^ currentSeed_ ^ currentPauseId_;
    pausePanelState_ = PausePanelState::OutgoingRequest;
    queuePauseActionRequestPacket(pendingOutgoingPauseAction_);
    return true;
}

bool GameController::handleGameOverInput(const InputEvent& event) {
    if (gameOverPanelState_ == GameOverPanelState::WaitingForRemote ||
        gameOverPanelState_ == GameOverPanelState::OutgoingRequest) {
        return false;
    }

    if (gameOverPanelState_ == GameOverPanelState::Disconnected) {
        if (isSelectEvent(event)) {
            returnToLobby();
            return true;
        }
        return false;
    }

    if (gameOverPanelState_ == GameOverPanelState::IncomingRequest) {
        if (isNavigationEvent(event)) {
            gameOverConfirmAcceptSelected_ = !gameOverConfirmAcceptSelected_;
            return true;
        }

        if (!isSelectEvent(event) || !pendingIncomingPostGameAction_.valid) {
            return false;
        }

        queuePostGameActionReplyPacket(pendingIncomingPostGameAction_, gameOverConfirmAcceptSelected_);
        if (gameOverConfirmAcceptSelected_) {
            const PostGameActionState accepted = pendingIncomingPostGameAction_;
            pendingIncomingPostGameAction_ = {};
            executePostGameAction(accepted.action, accepted.restartSeed);
        } else {
            pendingIncomingPostGameAction_ = {};
            updatePostGameState();
        }
        return true;
    }

    if (event.type != InputEventType::Press && event.type != InputEventType::Repeat) {
        return false;
    }

    if (isNavigationEvent(event)) {
        const int8_t delta = navigationDelta(event);
        postGameSelection_ = nextEnumValue(postGameSelection_, delta, 2);
        return true;
    }

    if (!isSelectEvent(event)) {
        return false;
    }

    pendingOutgoingPostGameAction_ = {};
    pendingOutgoingPostGameAction_.valid = true;
    pendingOutgoingPostGameAction_.requestId = nextPostGameRequestId_++;
    pendingOutgoingPostGameAction_.action = postGameSelection_;
    pendingOutgoingPostGameAction_.restartSeed = (millis() << 1) ^ currentSeed_ ^ nextPostGameRequestId_;
    gameOverPanelState_ = GameOverPanelState::OutgoingRequest;
    queuePostGameActionRequestPacket(pendingOutgoingPostGameAction_);
    return true;
}

bool GameController::handlePacket(const NetPacket& packet) {
    switch (packet.header.type) {
        case PacketType::Presence:
            return handlePresencePacket(packet);
        case PacketType::Ack:
            return false;
        case PacketType::LobbySettings:
            return handleLobbySettingsPacket(packet);
        case PacketType::StartGameRequest:
            return handleStartGameRequestPacket(packet);
        case PacketType::StartGameReply:
            return handleStartGameReplyPacket(packet);
        case PacketType::Pause:
            return handlePausePacket(packet);
        case PacketType::PauseActionRequest:
            return handlePauseActionRequestPacket(packet);
        case PacketType::PauseActionReply:
            return handlePauseActionReplyPacket(packet);
        case PacketType::PostGameActionRequest:
            return handlePostGameActionRequestPacket(packet);
        case PacketType::PostGameActionReply:
            return handlePostGameActionReplyPacket(packet);
        case PacketType::Garbage:
            return handleGarbagePacket(packet);
        case PacketType::GameOver:
            return handleGameOverPacket(packet);
    }

    return false;
}

bool GameController::handlePresencePacket(const NetPacket& packet) {
    remoteOnline_ = true;
    remoteDeviceId_ = packet.header.senderId;
    remotePresenceState_ = packet.payload.presence.presenceState;
    strncpy(remoteUsername_, packet.payload.presence.username, MAX_USERNAME_LEN);
    remoteUsername_[MAX_USERNAME_LEN] = '\0';
    return phase_ == GamePhase::Lobby;
}

bool GameController::handleLobbySettingsPacket(const NetPacket& packet) {
    if (phase_ != GamePhase::Lobby || lobbyModalState_ != ModalState::None) {
        return false;
    }

    const uint16_t incomingRevision = packet.payload.lobbySettings.revision;
    const uint16_t incomingOwnerId = packet.header.senderId;
    const bool accept = incomingRevision > settingsRevision_ ||
                        (incomingRevision == settingsRevision_ && incomingOwnerId > settingsOwnerId_);

    if (!accept) {
        return false;
    }

    matchSettings_ = packet.payload.lobbySettings.settings;
    settingsRevision_ = incomingRevision;
    settingsOwnerId_ = incomingOwnerId;
    return true;
}

bool GameController::handleStartGameRequestPacket(const NetPacket& packet) {
    if (phase_ != GamePhase::Lobby || lobbyModalState_ != ModalState::None) {
        queueStartGameReplyPacket(packet.payload.startGameRequest.requestId, false);
        return false;
    }

    pendingIncomingStart_ = {};
    pendingIncomingStart_.valid = true;
    pendingIncomingStart_.requestId = packet.payload.startGameRequest.requestId;
    pendingIncomingStart_.settingsRevision = packet.payload.startGameRequest.settingsRevision;
    pendingIncomingStart_.settingsOwnerId = packet.payload.startGameRequest.settingsOwnerId;
    pendingIncomingStart_.settings = packet.payload.startGameRequest.settings;
    pendingIncomingStart_.seed = packet.payload.startGameRequest.seed;
    lobbyModalState_ = ModalState::IncomingRequest;
    lobbyConfirmAcceptSelected_ = true;
    return true;
}

bool GameController::handleStartGameReplyPacket(const NetPacket& packet) {
    if (!pendingOutgoingStart_.valid ||
        lobbyModalState_ != ModalState::OutgoingRequest ||
        packet.payload.startGameReply.requestId != pendingOutgoingStart_.requestId) {
        return false;
    }

    const bool accepted = packet.payload.startGameReply.accepted;
    const StartRequestState outgoing = pendingOutgoingStart_;
    pendingOutgoingStart_ = {};
    lobbyModalState_ = ModalState::None;

    if (accepted) {
        beginGame(outgoing.seed, outgoing.settings);
    }

    return true;
}

bool GameController::handlePausePacket(const NetPacket& packet) {
    if (phase_ != GamePhase::Gameplay || localTerminal_) {
        return false;
    }

    enterPaused(packet.payload.pause.pauseId);
    return true;
}

bool GameController::handlePauseActionRequestPacket(const NetPacket& packet) {
    if (phase_ != GamePhase::Paused ||
        currentPauseId_ != packet.payload.pauseActionRequest.pauseId ||
        pausePanelState_ != PausePanelState::Menu) {
        PauseActionState reject = {};
        reject.valid = true;
        reject.pauseId = packet.payload.pauseActionRequest.pauseId;
        reject.requestId = packet.payload.pauseActionRequest.requestId;
        reject.action = packet.payload.pauseActionRequest.action;
        reject.restartSeed = packet.payload.pauseActionRequest.restartSeed;
        queuePauseActionReplyPacket(reject, false);
        return false;
    }

    pendingIncomingPauseAction_ = {};
    pendingIncomingPauseAction_.valid = true;
    pendingIncomingPauseAction_.pauseId = packet.payload.pauseActionRequest.pauseId;
    pendingIncomingPauseAction_.requestId = packet.payload.pauseActionRequest.requestId;
    pendingIncomingPauseAction_.action = packet.payload.pauseActionRequest.action;
    pendingIncomingPauseAction_.restartSeed = packet.payload.pauseActionRequest.restartSeed;
    pausePanelState_ = PausePanelState::IncomingRequest;
    pauseConfirmAcceptSelected_ = true;
    return true;
}

bool GameController::handlePauseActionReplyPacket(const NetPacket& packet) {
    if (!pendingOutgoingPauseAction_.valid ||
        pausePanelState_ != PausePanelState::OutgoingRequest ||
        packet.payload.pauseActionReply.requestId != pendingOutgoingPauseAction_.requestId ||
        packet.payload.pauseActionReply.pauseId != pendingOutgoingPauseAction_.pauseId) {
        return false;
    }

    const bool accepted = packet.payload.pauseActionReply.accepted;
    const PauseActionState outgoing = pendingOutgoingPauseAction_;
    pendingOutgoingPauseAction_ = {};

    if (accepted) {
        executePauseAction(outgoing.action, outgoing.restartSeed);
    } else {
        pausePanelState_ = PausePanelState::Menu;
    }

    return true;
}

bool GameController::handlePostGameActionRequestPacket(const NetPacket& packet) {
    if (phase_ != GamePhase::GameOver ||
        !localTerminal_ ||
        !remoteTerminal_ ||
        disconnected_ ||
        gameOverPanelState_ != GameOverPanelState::Menu) {
        PostGameActionState reject = {};
        reject.valid = true;
        reject.requestId = packet.payload.postGameActionRequest.requestId;
        reject.action = packet.payload.postGameActionRequest.action;
        reject.restartSeed = packet.payload.postGameActionRequest.restartSeed;
        queuePostGameActionReplyPacket(reject, false);
        return false;
    }

    pendingIncomingPostGameAction_ = {};
    pendingIncomingPostGameAction_.valid = true;
    pendingIncomingPostGameAction_.requestId = packet.payload.postGameActionRequest.requestId;
    pendingIncomingPostGameAction_.action = packet.payload.postGameActionRequest.action;
    pendingIncomingPostGameAction_.restartSeed = packet.payload.postGameActionRequest.restartSeed;
    gameOverPanelState_ = GameOverPanelState::IncomingRequest;
    gameOverConfirmAcceptSelected_ = true;
    return true;
}

bool GameController::handlePostGameActionReplyPacket(const NetPacket& packet) {
    if (!pendingOutgoingPostGameAction_.valid ||
        gameOverPanelState_ != GameOverPanelState::OutgoingRequest ||
        packet.payload.postGameActionReply.requestId != pendingOutgoingPostGameAction_.requestId) {
        return false;
    }

    const bool accepted = packet.payload.postGameActionReply.accepted;
    const PostGameActionState outgoing = pendingOutgoingPostGameAction_;
    pendingOutgoingPostGameAction_ = {};

    if (accepted) {
        executePostGameAction(outgoing.action, outgoing.restartSeed);
    } else {
        updatePostGameState();
    }

    return true;
}

bool GameController::handleGameOverPacket(const NetPacket& packet) {
    remoteTerminal_ = true;
    remoteGameOverReason_ = packet.payload.gameOver.reason;
    remoteFinalScore_ = packet.payload.gameOver.finalScore;
    remoteLinesCleared_ = packet.payload.gameOver.linesCleared;

    if (phase_ == GamePhase::GameOver && localTerminal_) {
        updatePostGameState();
    }

    return true;
}

bool GameController::handleGarbagePacket(const NetPacket& packet) {
    if (phase_ != GamePhase::Gameplay || localTerminal_) {
        return false;
    }

    return handleStepResult(engine_.applyGarbage({
        packet.payload.garbage.lines,
        packet.payload.garbage.holeColumn
    }));
}

bool GameController::handleStepResult(const GameEngine::StepResult& result) {
    if (result.hasOutgoingGarbage) {
        queueGarbagePacket(result.outgoingGarbage);
    }

    if (result.gameOver) {
        finalizeLocalGameOver(engine_.gameOverReason());
    }

    return result.changed || result.gameOver;
}

bool GameController::queueOutboundPacket(const NetPacket& packet) {
    if (outboundCount_ >= OUTBOUND_PACKET_CAPACITY) {
        return false;
    }

    const uint8_t tail = (outboundHead_ + outboundCount_) % OUTBOUND_PACKET_CAPACITY;
    outboundPackets_[tail] = packet;
    ++outboundCount_;
    return true;
}

bool GameController::queueMusicEvent(MusicEventType type) {
    if (musicCount_ >= MUSIC_EVENT_CAPACITY) {
        return false;
    }

    const uint8_t tail = (musicHead_ + musicCount_) % MUSIC_EVENT_CAPACITY;
    musicEvents_[tail] = {type};
    ++musicCount_;
    return true;
}

void GameController::queuePresencePacket() {
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::Presence;
    packet.payload.presence.presenceState = presenceForPhase(phase_);
    strncpy(packet.payload.presence.username, userSettings_.username, MAX_USERNAME_LEN);
    packet.payload.presence.username[MAX_USERNAME_LEN] = '\0';
    (void)queueOutboundPacket(packet);
}

void GameController::queueLobbySettingsPacket() {
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::LobbySettings;
    packet.payload.lobbySettings.revision = settingsRevision_;
    packet.payload.lobbySettings.settings = matchSettings_;
    (void)queueOutboundPacket(packet);
}

void GameController::queueStartGameRequestPacket(uint32_t seed) {
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::StartGameRequest;
    packet.payload.startGameRequest = {
        pendingOutgoingStart_.requestId,
        settingsRevision_,
        settingsOwnerId_,
        matchSettings_,
        seed
    };
    (void)queueOutboundPacket(packet);
}

void GameController::queueStartGameReplyPacket(uint8_t requestId, bool accepted) {
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::StartGameReply;
    packet.payload.startGameReply = {
        requestId,
        accepted
    };
    (void)queueOutboundPacket(packet);
}

void GameController::queuePausePacket(uint8_t pauseId) {
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::Pause;
    packet.payload.pause = {pauseId};
    (void)queueOutboundPacket(packet);
}

void GameController::queuePauseActionRequestPacket(const PauseActionState& request) {
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::PauseActionRequest;
    packet.payload.pauseActionRequest = {
        request.pauseId,
        request.requestId,
        request.action,
        request.restartSeed
    };
    (void)queueOutboundPacket(packet);
}

void GameController::queuePauseActionReplyPacket(const PauseActionState& request, bool accepted) {
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::PauseActionReply;
    packet.payload.pauseActionReply = {
        request.pauseId,
        request.requestId,
        request.action,
        accepted
    };
    (void)queueOutboundPacket(packet);
}

void GameController::queuePostGameActionRequestPacket(const PostGameActionState& request) {
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::PostGameActionRequest;
    packet.payload.postGameActionRequest = {
        request.requestId,
        request.action,
        request.restartSeed
    };
    (void)queueOutboundPacket(packet);
}

void GameController::queuePostGameActionReplyPacket(const PostGameActionState& request, bool accepted) {
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::PostGameActionReply;
    packet.payload.postGameActionReply = {
        request.requestId,
        request.action,
        accepted
    };
    (void)queueOutboundPacket(packet);
}

void GameController::queueGarbagePacket(const GameEngine::GarbageAttack& garbage) {
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::Garbage;
    packet.payload.garbage = {
        garbage.lines,
        garbage.holeColumn
    };
    (void)queueOutboundPacket(packet);
}

void GameController::queueGameOverPacket() {
    if (localGameOverPacketQueued_) {
        return;
    }

    const PlayerRenderState state = engine_.renderState();
    NetPacket packet = {};
    packet.header.protocolVersion = NET_PROTOCOL_VERSION;
    packet.header.type = PacketType::GameOver;
    packet.payload.gameOver = {
        localGameOverReason_,
        state.score,
        state.linesCleared
    };

    localGameOverPacketQueued_ = queueOutboundPacket(packet);
}

void GameController::queueUserLoad(const char* username) {
    StorageRequest request = {};
    request.operation = StorageOperation::Load;
    request.key = StorageKey::UserSettings;
    strncpy(request.localUsername, username, MAX_USERNAME_LEN);
    request.localUsername[MAX_USERNAME_LEN] = '\0';

    if (xQueueSend(g_storageRequestQueue, &request, 0) == pdTRUE) {
        storageBusy_ = true;
        pendingStorageOperation_ = StorageOperation::Load;
        pendingStorageMode_ = usernameEntryMode_;
        usernameEntryMessage_ = UsernameEntryMessage::None;
    } else {
        usernameEntryMessage_ = UsernameEntryMessage::StorageFailed;
    }
}

void GameController::queueUserSave(const UserSettings& settings) {
    StorageRequest request = {};
    request.operation = StorageOperation::Save;
    request.key = StorageKey::UserSettings;
    strncpy(request.localUsername, settings.username, MAX_USERNAME_LEN);
    request.localUsername[MAX_USERNAME_LEN] = '\0';
    request.payload.userSettings = settings;

    if (xQueueSend(g_storageRequestQueue, &request, 0) == pdTRUE) {
        storageBusy_ = true;
        pendingStorageOperation_ = StorageOperation::Save;
        pendingStorageMode_ = usernameEntryMode_;
        pendingSaveSettings_ = settings;
        usernameEntryMessage_ = UsernameEntryMessage::None;
    } else {
        usernameEntryMessage_ = UsernameEntryMessage::StorageFailed;
    }
}

void GameController::enterWelcome() {
    phase_ = GamePhase::Welcome;
    welcomeSelection_ = WelcomeMenuItem::SignIn;
    resetStoragePending();
    usernameEntryMessage_ = UsernameEntryMessage::None;
    pendingUserSettingsExit_ = UserSettingsExitAction::None;
    displayConfigDirty_ = true;
    remoteOnline_ = false;
    remotePresenceState_ = PresenceState::NotInLobby;
    queuePresencePacket();
}

void GameController::enterUsernameEntry(UsernameEntryMode mode) {
    phase_ = GamePhase::UsernameEntry;
    usernameEntryMode_ = mode;
    usernameEntrySelection_ = UsernameEntryItem::Letters;
    usernameEntryMessage_ = UsernameEntryMessage::None;
    strncpy(usernameDraft_, "A", MAX_USERNAME_LEN);
    usernameDraft_[1] = '\0';
    usernameCursorIndex_ = 0;
    usernameSlotActive_ = false;
    resetStoragePending();
}

void GameController::enterUserSettings(const UserSettings& settings) {
    userSettings_ = settings;
    userSettings_.username[MAX_USERNAME_LEN] = '\0';
    userSettings_.nextPreviewCount = min(userSettings_.nextPreviewCount, MAX_NEXT_PIECES);
    draftUserSettings_ = userSettings_;
    phase_ = GamePhase::UserSettings;
    userSettingsSelection_ = UserSettingsMenuItem::Theme;
    pendingUserSettingsExit_ = UserSettingsExitAction::None;
    userSettingsConfirmContinueSelected_ = false;
    displayConfigDirty_ = true;
    resetStoragePending();
}

void GameController::joinLobbyFromSettings() {
    draftUserSettings_ = userSettings_;
    phase_ = GamePhase::Lobby;
    lobbySelection_ = LobbyMenuItem::GarbageEnabled;
    lobbyModalState_ = ModalState::None;
    lobbyConfirmAcceptSelected_ = true;
    pendingIncomingStart_ = {};
    pendingOutgoingStart_ = {};
    queuePresencePacket();
}

void GameController::signOutFromSettings() {
    userSettings_ = DefaultSettings::userSettings();
    draftUserSettings_ = userSettings_;
    enterWelcome();
}

void GameController::beginGame(uint32_t seed, const MatchSettings& settings) {
    matchSettings_ = settings;
    currentSeed_ = seed;
    engine_.begin({
        seed,
        matchSettings_,
        userSettings_
    });

    phase_ = GamePhase::Gameplay;
    lobbyModalState_ = ModalState::None;
    pendingIncomingStart_ = {};
    pendingOutgoingStart_ = {};

    currentPauseId_ = 0;
    pauseSelection_ = PauseMenuAction::Resume;
    pausePanelState_ = PausePanelState::Menu;
    pauseConfirmAcceptSelected_ = true;
    pendingIncomingPauseAction_ = {};
    pendingOutgoingPauseAction_ = {};

    localTerminal_ = false;
    remoteTerminal_ = false;
    localFinishedBeforeRemote_ = false;
    localWon_ = false;
    disconnected_ = false;
    localGameOverReason_ = GameOverReason::TopOut;
    remoteGameOverReason_ = GameOverReason::TopOut;
    remoteFinalScore_ = 0;
    remoteLinesCleared_ = 0;
    postGameSelection_ = PostGameMenuAction::Restart;
    gameOverPanelState_ = GameOverPanelState::WaitingForRemote;
    gameOverConfirmAcceptSelected_ = true;
    pendingIncomingPostGameAction_ = {};
    pendingOutgoingPostGameAction_ = {};
    localGameOverPacketQueued_ = false;

    (void)queueMusicEvent(matchSettings_.musicEnabled ? MusicEventType::Start : MusicEventType::Stop);
    queuePresencePacket();
}

void GameController::returnToLobby() {
    phase_ = GamePhase::Lobby;
    lobbyModalState_ = ModalState::None;
    lobbyConfirmAcceptSelected_ = true;
    pendingIncomingStart_ = {};
    pendingOutgoingStart_ = {};

    currentPauseId_ = 0;
    pauseSelection_ = PauseMenuAction::Resume;
    pausePanelState_ = PausePanelState::Menu;
    pauseConfirmAcceptSelected_ = true;
    pendingIncomingPauseAction_ = {};
    pendingOutgoingPauseAction_ = {};

    localTerminal_ = false;
    remoteTerminal_ = false;
    localFinishedBeforeRemote_ = false;
    localWon_ = false;
    disconnected_ = false;
    localGameOverReason_ = GameOverReason::TopOut;
    remoteGameOverReason_ = GameOverReason::TopOut;
    remoteFinalScore_ = 0;
    remoteLinesCleared_ = 0;
    postGameSelection_ = PostGameMenuAction::Restart;
    gameOverPanelState_ = GameOverPanelState::WaitingForRemote;
    gameOverConfirmAcceptSelected_ = true;
    pendingIncomingPostGameAction_ = {};
    pendingOutgoingPostGameAction_ = {};
    localGameOverPacketQueued_ = false;

    (void)queueMusicEvent(MusicEventType::Stop);
    queuePresencePacket();
}

void GameController::enterPaused(uint8_t pauseId) {
    phase_ = GamePhase::Paused;
    currentPauseId_ = pauseId;
    pauseSelection_ = PauseMenuAction::Resume;
    pausePanelState_ = PausePanelState::Menu;
    pauseConfirmAcceptSelected_ = true;
    pendingIncomingPauseAction_ = {};
    pendingOutgoingPauseAction_ = {};
    (void)queueMusicEvent(MusicEventType::Pause);
    queuePresencePacket();
}

void GameController::executePauseAction(PauseMenuAction action, uint32_t restartSeed) {
    pendingIncomingPauseAction_ = {};
    pendingOutgoingPauseAction_ = {};

    switch (action) {
        case PauseMenuAction::Resume:
            phase_ = GamePhase::Gameplay;
            (void)queueMusicEvent(matchSettings_.musicEnabled ? MusicEventType::Resume : MusicEventType::Stop);
            queuePresencePacket();
            return;
        case PauseMenuAction::Restart:
            beginGame(restartSeed, matchSettings_);
            return;
        case PauseMenuAction::Quit:
            returnToLobby();
            return;
    }
}

void GameController::executePostGameAction(PostGameMenuAction action, uint32_t restartSeed) {
    pendingIncomingPostGameAction_ = {};
    pendingOutgoingPostGameAction_ = {};

    switch (action) {
        case PostGameMenuAction::Restart:
            beginGame(restartSeed, matchSettings_);
            return;
        case PostGameMenuAction::Quit:
            returnToLobby();
            return;
    }
}

void GameController::finalizeLocalGameOver(GameOverReason reason) {
    if (localTerminal_) {
        return;
    }

    localTerminal_ = true;
    localGameOverReason_ = reason;
    localFinishedBeforeRemote_ = !remoteTerminal_;
    phase_ = GamePhase::GameOver;
    (void)queueMusicEvent(MusicEventType::Stop);
    queueGameOverPacket();
    queuePresencePacket();
    updatePostGameState();
}

void GameController::updatePostGameState() {
    if (phase_ != GamePhase::GameOver || !localTerminal_) {
        return;
    }

    if (disconnected_) {
        gameOverPanelState_ = GameOverPanelState::Disconnected;
        return;
    }

    if (!remoteTerminal_) {
        gameOverPanelState_ = GameOverPanelState::WaitingForRemote;
        return;
    }

    localWon_ = !localFinishedBeforeRemote_;

    if (pendingIncomingPostGameAction_.valid) {
        gameOverPanelState_ = GameOverPanelState::IncomingRequest;
    } else if (pendingOutgoingPostGameAction_.valid) {
        gameOverPanelState_ = GameOverPanelState::OutgoingRequest;
    } else {
        gameOverPanelState_ = GameOverPanelState::Menu;
    }
}

bool GameController::isNavigationEvent(const InputEvent& event) const {
    return (event.type == InputEventType::Press || event.type == InputEventType::Repeat) &&
           event.button != PhysicalButton::Center;
}

int8_t GameController::navigationDelta(const InputEvent& event) const {
    if (isMenuLeft(event.button) || isMenuUp(event.button)) {
        return -1;
    }

    if (isMenuRight(event.button) || isMenuDown(event.button)) {
        return 1;
    }

    return 0;
}

bool GameController::isSelectEvent(const InputEvent& event) const {
    return event.button == PhysicalButton::Center && event.type == InputEventType::Press;
}

bool GameController::userSettingsDirty() const {
    return !userSettingsEqual(userSettings_, draftUserSettings_);
}

bool GameController::applyUsernameSlotDelta(int8_t delta) {
    char current = usernameDraft_[usernameCursorIndex_];
    const bool allowBlank = usernameCursorIndex_ > 0;

    int16_t value;
    if (current == '\0' || current == USERNAME_BLANK) {
        value = allowBlank ? 26 : 0;
    } else {
        value = current - 'A';
    }

    const int16_t count = allowBlank ? 27 : 26;
    value += delta;
    while (value < 0) {
        value += count;
    }
    while (value >= count) {
        value -= count;
    }

    if (allowBlank && value == 26) {
        usernameDraft_[usernameCursorIndex_] = '\0';
        if (usernameCursorIndex_ > 0) {
            --usernameCursorIndex_;
        }
    } else {
        usernameDraft_[usernameCursorIndex_] = static_cast<char>('A' + value);
        if (usernameCursorIndex_ == usernameLength(usernameDraft_) - 1 &&
            usernameCursorIndex_ + 1 < MAX_USERNAME_LEN) {
            usernameDraft_[usernameCursorIndex_ + 1] = '\0';
        }
    }

    normalizeUsernameCursorAfterEdit();
    usernameEntryMessage_ = UsernameEntryMessage::None;
    return true;
}

void GameController::normalizeUsernameCursorAfterEdit() {
    usernameDraft_[MAX_USERNAME_LEN] = '\0';
    uint8_t length = usernameLength(usernameDraft_);
    if (length == 0) {
        usernameDraft_[0] = 'A';
        usernameDraft_[1] = '\0';
        length = 1;
    }

    for (uint8_t i = 0; i < MAX_USERNAME_LEN; ++i) {
        if (usernameDraft_[i] == USERNAME_BLANK) {
            usernameDraft_[i] = '\0';
            length = i;
            break;
        }
    }

    if (length == 0) {
        usernameDraft_[0] = 'A';
        usernameDraft_[1] = '\0';
        length = 1;
    }

    const uint8_t maxCursor = min<uint8_t>(MAX_USERNAME_LEN - 1, length);
    if (usernameCursorIndex_ > maxCursor) {
        usernameCursorIndex_ = maxCursor;
    }
}

void GameController::setUsernameDraftFromCurrentLength() {
    const uint8_t length = usernameLength(usernameDraft_);
    if (usernameCursorIndex_ == length && length < MAX_USERNAME_LEN) {
        usernameDraft_[usernameCursorIndex_] = 'A';
        usernameDraft_[usernameCursorIndex_ + 1] = '\0';
    }
}

void GameController::resetStoragePending() {
    storageBusy_ = false;
    pendingStorageOperation_ = StorageOperation::Load;
    pendingStorageMode_ = UsernameEntryMode::SignIn;
}

bool GameController::mapsToAction(PhysicalButton button, GameEngine::Action& action) const {
    if (button == userSettings_.buttonMapping.moveLeft) {
        action = GameEngine::Action::MoveLeft;
        return true;
    }

    if (button == userSettings_.buttonMapping.moveRight) {
        action = GameEngine::Action::MoveRight;
        return true;
    }

    if (button == userSettings_.buttonMapping.hardDrop) {
        action = GameEngine::Action::HardDrop;
        return true;
    }

    if (button == userSettings_.buttonMapping.rotateCw) {
        action = GameEngine::Action::RotateCw;
        return true;
    }

    if (button == userSettings_.buttonMapping.rotateCcw) {
        action = GameEngine::Action::RotateCcw;
        return true;
    }

    if (button == userSettings_.buttonMapping.hold) {
        action = GameEngine::Action::Hold;
        return true;
    }

    return false;
}

bool GameController::mapsToRepeatAction(PhysicalButton button, GameEngine::Action& action) const {
    if (button == userSettings_.buttonMapping.moveLeft) {
        action = GameEngine::Action::MoveLeft;
        return true;
    }

    if (button == userSettings_.buttonMapping.moveRight) {
        action = GameEngine::Action::MoveRight;
        return true;
    }

    return false;
}
