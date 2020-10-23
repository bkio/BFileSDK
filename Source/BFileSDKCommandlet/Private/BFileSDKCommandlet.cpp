/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileSDKCommandlet.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "ProcessUnrealOptimizerCommandlet.h"

DEFINE_LOG_CATEGORY(LogCommandletPlugin);

void FBFileSDKCommandletModule::StartupModule()
{
}

IMPLEMENT_MODULE(FBFileSDKCommandletModule, BFileSDKCommandlet);