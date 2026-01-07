#include "FicsitChatModule.h"
#include "FGChatManager.h"
#include "FGPlayerState.h"
#include "FicsitChatWorldModule.h"
#include "Module/GameInstanceModule.h"
#include "Module/WorldModuleManager.h"
#include "Patching/NativeHookManager.h"
#include "Reflection/BlueprintReflectionLibrary.h"

#define LOCTEXT_NAMESPACE "FFicsitChatModule"

DEFINE_LOG_CATEGORY(LogFicsitChat);

void FFicsitChatModule::StartupModule() {
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Register hooks
	RegisterHooks();
}

void FFicsitChatModule::ShutdownModule() {
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FFicsitChatModule::RegisterHooks() {
#if !WITH_EDITOR
	AFGChatManager *afgChatManager = GetMutableDefault<AFGChatManager>();
	
	// Define the lambda separately to avoid macro preprocessor issues with commas
	auto chatMessageHandler = [](AFGChatManager *self, FChatMessageStruct newMessage) {
		std::string joinPrefix = "<PlayerName/>";
		std::string discordPrefix = "(from discord)";
		std::string userName = TCHAR_TO_UTF8(*newMessage.MessageSender.ToString());
		std::string message = TCHAR_TO_UTF8(*newMessage.MessageText.ToString());

		UE_LOG(LogFicsitChat, Display, TEXT("Chat message by %s sent to all clients: %s"), *newMessage.MessageSender.ToString(), *newMessage.MessageText.ToString());

		FFicsitChat_ConfigStruct config = FFicsitChat_ConfigStruct::GetActiveConfig((UFicsitChatWorldModule *)self->GetWorld());
		UFicsitChatWorldModule *worldModule = (UFicsitChatWorldModule *)self->GetWorld()->GetSubsystem<UWorldModuleManager>()->FindModule(TEXT("FicsitChat"));

		// Check if worldModule exists and bot is initialized
		if (!worldModule || !worldModule->bot.IsValid()) {
			UE_LOG(LogFicsitChat, Warning, TEXT("Discord bot not initialized, cannot send message"));
			return;
		}

		// Message is from Discord
		if (message.compare(0, discordPrefix.length(), discordPrefix) == 0) {
			if (config.EnableDebugLogging) {
				UE_LOG(LogFicsitChat, Verbose, TEXT("Ignoring message from discord"));
			}
			return;
		}

		// Message is join / leave game message
		if (message.compare(0, joinPrefix.length(), joinPrefix) == 0) {
			if (config.EnableDebugLogging) {
				UE_LOG(LogFicsitChat, Verbose, TEXT("Message starts with join/leave prefix: %s"), *FString(joinPrefix.c_str()));
				UE_LOG(LogFicsitChat, Verbose, TEXT("Full message: %s"), *FString(message.c_str()));
			}
			
			// FIXME: Figure out a proper way to localize this
			std::string joinMessages[] = {
				"has joined the game!", // en_US
				"entrou no jogo!" // pt_BR
			};
			std::string leaveMessages[] = {
				"has left the game!", // en_US
				"saiu do jogo!" // pt_BR
			};
			int joinMsgCount = sizeof(joinMessages) / sizeof(joinMessages[0]);
			int leaveMsgCount = sizeof(leaveMessages) / sizeof(leaveMessages[0]);
			
			// Extract the part after the prefix for comparison
			std::string messageSuffix = message.substr(joinPrefix.length() + 1);
			if (config.EnableDebugLogging) {
				UE_LOG(LogFicsitChat, Verbose, TEXT("Message suffix (after prefix): %s"), *FString(messageSuffix.c_str()));
			}
			
			bool foundJoinMatch = false;
			for (int i = 0; i < joinMsgCount; i++) {
				if (config.EnableDebugLogging) {
					UE_LOG(LogFicsitChat, Verbose, TEXT("Comparing with join message [%d]: %s"), i, *FString(joinMessages[i].c_str()));
				}
				
				if (messageSuffix.compare(0, joinMessages[i].length(), joinMessages[i]) == 0) {
					foundJoinMatch = true;
					if (config.EnableDebugLogging) {
						UE_LOG(LogFicsitChat, Verbose, TEXT("MATCH FOUND: Join message detected (index %d)"), i);
					}
					
					if (!config.HasJoinedMessage) {
						if (config.EnableDebugLogging) {
							UE_LOG(LogFicsitChat, Verbose, TEXT("HasJoinedMessage is false, suppressing join message"));
						}
						return;
					}
					message = joinMessages[0] + " :white_check_mark:";
					if (config.EnableDebugLogging) {
						UE_LOG(LogFicsitChat, Verbose, TEXT("Normalized join message to: %s"), *FString(message.c_str()));
					}
					break;
				}
			}
			
			if (!foundJoinMatch) {
				bool foundLeaveMatch = false;
				for (int i = 0; i < leaveMsgCount; i++) {
					if (config.EnableDebugLogging) {
						UE_LOG(LogFicsitChat, Verbose, TEXT("Comparing with leave message [%d]: %s"), i, *FString(leaveMessages[i].c_str()));
					}
					
					if (messageSuffix.compare(0, leaveMessages[i].length(), leaveMessages[i]) == 0) {
						foundLeaveMatch = true;
						if (config.EnableDebugLogging) {
							UE_LOG(LogFicsitChat, Verbose, TEXT("MATCH FOUND: Leave message detected (index %d)"), i);
						}
						
						if (!config.HasLeftMessage) {
							if (config.EnableDebugLogging) {
								UE_LOG(LogFicsitChat, Verbose, TEXT("HasLeftMessage is false, suppressing leave message"));
							}
							return;
						}
						message = leaveMessages[0] + " :no_entry:";
						if (config.EnableDebugLogging) {
							UE_LOG(LogFicsitChat, Verbose, TEXT("Normalized leave message to: %s"), *FString(message.c_str()));
						}
						break;
					}
				}
				
				if (!foundLeaveMatch && config.EnableDebugLogging) {
					UE_LOG(LogFicsitChat, Verbose, TEXT("NO MATCH: Message has join/leave prefix but doesn't match any known patterns"));
				}
			}
		}

		dpp::embed embed = dpp::embed().set_color(dpp::colors::orange).set_title(userName).set_description(message);
		if (config.ShowFooter) {
			std::string footerText = TCHAR_TO_UTF8(*config.FooterText);
			if (config.FooterText.IsEmpty()) {
				footerText = "If you're tired, just remember you can buy a FICSIT™ Coffee Cup at the AWESOME Shop!";
			}
			embed.set_footer(dpp::embed_footer().set_text(footerText));
		}

		worldModule->bot->message_create(dpp::message(dpp::snowflake::snowflake(TCHAR_TO_UTF8(*config.ChannelId)), embed));
	};
	
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGChatManager::AddChatMessageToReceived, afgChatManager, chatMessageHandler);
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_GAME_MODULE(FFicsitChatModule, FicsitChat)
