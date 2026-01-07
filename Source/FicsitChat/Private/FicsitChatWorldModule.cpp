#include "FicsitChatWorldModule.h"
#include "Configuration/ConfigManager.h"

UFicsitChatWorldModule::UFicsitChatWorldModule() {
#if !WITH_EDITOR
	this->bRootModule = true;
#endif
}

void UFicsitChatWorldModule::BeginDestroy() {
	Super::BeginDestroy();

#if !WITH_EDITOR
	UE_LOG(LogFicsitChat, Display, TEXT("BeginDestroy"));
#endif
}

// Runs on game world load
void UFicsitChatWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase) {
	std::string discordPrefix = "(from discord)";

	if (Phase != ELifecyclePhase::INITIALIZATION)
		return;

	UE_LOG(LogFicsitChat, Display, TEXT("DispatchLifecycleEvent"));

	// Get mod config
	FFicsitChat_ConfigStruct config = FFicsitChat_ConfigStruct::GetActiveConfig(GetWorld());

	if (config.EnableDebugLogging) {
		UE_LOG(LogFicsitChat, Display, TEXT("FicsitChat started with DebugLogging enabled"));
	}

	// Start Discord bot
	if (!ValidateBotToken(*config.BotToken)) {
		return;
	}

	UE_LOG(LogFicsitChat, Display, TEXT("Starting the Discord bot. If the game crashes due to FicsitChat or DPP, check if you have the right intents set (check on https://discord.com/developers), and check if your bot token is valid."));
	bot = MakeShared<dpp::cluster>(TCHAR_TO_UTF8(*config.BotToken), dpp::i_default_intents | dpp::i_message_content);

	bot->on_ready([&, config](auto event) {
		if (dpp::run_once<struct register_bot_commands>()) {
			bot->global_command_create(dpp::slashcommand("ping", "Ping pong!", bot->me.id));
		}

		botUsername = bot->me.username.c_str();
		botDiscriminator = bot->me.discriminator;
		if (config.EnableDebugLogging) {
			UE_LOG(LogFicsitChat, Verbose, TEXT("Discord bot connected! Username: %s, Discriminator: %d"), *botUsername, botDiscriminator);
		}
	});

	bot->on_message_create([this, config, discordPrefix](const dpp::message_create_t &event) {
		if (config.EnableDebugLogging) {
			UE_LOG(LogFicsitChat, Verbose, TEXT("on_message_create fired! Channel ID: %s"), *FString(std::to_string(event.msg.channel_id).c_str()));
		}
		
		FString messageChannelId = FString(std::to_string(event.msg.channel_id).c_str());
		if (messageChannelId != config.ChannelId) {
			if (config.EnableDebugLogging) {
				UE_LOG(LogFicsitChat, Verbose, TEXT("Channel mismatch: got %s, expected %s"), *messageChannelId, *config.ChannelId);
			}
			return;
		}

		FString messageAuthor = event.msg.author.username.c_str();
		uint16_t messageAuthorDiscriminator = event.msg.author.discriminator;
		if (messageAuthor == botUsername && messageAuthorDiscriminator == botDiscriminator) {
			if (config.EnableDebugLogging) {
				UE_LOG(LogFicsitChat, Verbose, TEXT("Ignoring message from self"));
			}
			return;
		}

		// Convert event.msg.content (std::string) to FString
		FString messageContent = FString(discordPrefix.c_str()) + TEXT(" ") + FString(UTF8_TO_TCHAR(event.msg.content.c_str()));
		
		if (config.EnableDebugLogging) {
			UE_LOG(LogFicsitChat, Verbose, TEXT("Dispatching Discord message to game thread: %s: %s"), *messageAuthor, *messageContent);
		}
		AsyncTask(ENamedThreads::GameThread, [this, messageContent, messageAuthor]() { SendMessageToGame(messageContent, messageAuthor); });
	});

	bot->on_slashcommand([](auto event) {
		if (event.command.get_command_name() == "ping") {
			event.reply("Pong!");
		}
	});

	bot->start(dpp::st_return);
}

bool UFicsitChatWorldModule::ValidateBotToken(FString botToken) {
	std::string botTokenString = TCHAR_TO_UTF8(*botToken);

	if (botToken.Len() < 40 || botToken.Len() > 100) {
		UE_LOG(LogFicsitChat, Warning, TEXT("Failed to validate the Discord bot token. The Discord bot token's length needs to be in between 40 and 100 characters long."));
		return false;
	} else if (botTokenString.find(std::string("BOT_TOKEN_HERE")) != std::string::npos) {
		UE_LOG(LogFicsitChat, Warning,
			   TEXT("Failed to validate the Discord bot token. The Discord bot token is set to the default value, which is invalid.\nPlease change it in the mod's configuration and reload your save. The Discord bot will not be started "
					"until then."));
		return false;
	}

	return true;
}

void UFicsitChatWorldModule::SendMessageToGame(FString messageContent, FString messageAuthor) {
	FFicsitChat_ConfigStruct config = FFicsitChat_ConfigStruct::GetActiveConfig(GetWorld());
	
	if (config.EnableDebugLogging) {
		UE_LOG(LogFicsitChat, Verbose, TEXT("SendMessageToGame called: Author=%s, Content=%s"), *messageAuthor, *messageContent);
	}

	FChatMessageStruct chatMessageStruct{};
	chatMessageStruct.MessageText = FText::FromString(messageContent);
	chatMessageStruct.MessageType = EFGChatMessageType::CMT_PlayerMessage;
	chatMessageStruct.ServerTimeStamp = GetWorld()->TimeSeconds;
	chatMessageStruct.MessageSender = FText::FromString(messageAuthor);
	chatMessageStruct.MessageSenderColor = FLinearColor(config.ChatMessageColor.Red, config.ChatMessageColor.Green, config.ChatMessageColor.Blue);

	// Use BroadcastChatMessage which properly replicates to all clients via NetMulticast RPC
	AFGChatManager *chatManager = AFGChatManager::Get(GetWorld());
	if (chatManager) {
		if (config.EnableDebugLogging) {
			UE_LOG(LogFicsitChat, Verbose, TEXT("Broadcasting message to all clients via BroadcastChatMessage"));
		}
		chatManager->BroadcastChatMessage(chatMessageStruct, nullptr);
		if (config.EnableDebugLogging) {
			UE_LOG(LogFicsitChat, Verbose, TEXT("Broadcast complete"));
		}
	} else {
		UE_LOG(LogFicsitChat, Error, TEXT("Chat manager is null!"));
	}
}
