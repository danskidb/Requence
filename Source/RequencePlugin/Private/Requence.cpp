// Fill out your copyright notice in the Description page of Project Settings.

#include "Requence.h"
#include "UObjectIterator.h"
#include "JsonObject.h"
#include "JsonWriter.h"
#include "JsonSerializer.h"
#include "PlatformFilemanager.h"
#include "FileHelper.h"

URequence::URequence() {}

bool URequence::LoadUnrealInput()
{
	ClearDevicesAndAxises();

	UInputSettings* Settings = GetMutableDefault<UInputSettings>();
	if (!Settings) { return false; }

	//ACTIONS
	const TArray<FInputActionKeyMapping>& _Actions = Settings->ActionMappings;
	for (const FInputActionKeyMapping& Each : _Actions)
	{
		FRequenceInputAction FRIAc = FRequenceInputAction(Each);
		Actions.Add(FRIAc);

 		//Grab device
		ERequenceDeviceType erdt = URequenceDevice::GetDeviceTypeByKeyString(FRIAc.KeyString);
		URequenceDevice* device = GetDeviceByType(erdt);
		if (IsValid(device)) 
		{	//Add to current device
			device->AddAction(FRIAc);
		} 
		else
		{	//Create new device and add key.
			device = CreateDevice(FRIAc.KeyString);
			device->AddAction(FRIAc);
		}
	}

	//AXISES
	const TArray<FInputAxisKeyMapping>& _Axises = Settings->AxisMappings;
	for (const FInputAxisKeyMapping& Each : _Axises)
	{
		FRequenceInputAxis FRIAx = FRequenceInputAxis(Each);
		Axises.Add(FRIAx);

		//Grab device
		ERequenceDeviceType erdt = URequenceDevice::GetDeviceTypeByKeyString(FRIAx.KeyString);
		URequenceDevice* device = GetDeviceByType(erdt);
		if (IsValid(device))
		{	//Add to current device
			device->AddAxis(FRIAx);
		}
		else
		{	//Create new device and add key.
			device = CreateDevice(FRIAx.KeyString);
			device->AddAxis(FRIAx);
		}
	}

	//Fill FullAxis/ActionList
	for (FRequenceInputAxis ax : Axises)
	{
		if (!FullAxisList.Contains(ax.AxisName)) { FullAxisList.Add(ax.AxisName); }
	}
	for (FRequenceInputAction ac : Actions)
	{
		if (!FullActionList.Contains(ac.ActionName)) { FullActionList.Add(ac.ActionName); }
	}

	//todo: Separate unique device into separate devices.

	//Add empty bindings so every device has all the mappings. Also sort alphabetically.
	for (URequenceDevice* d : Devices)
	{
		AddAllEmpty(d);

		//Sort it!
		d->SortAlphabetically();
		d->CompactifyAllKeyNames();
	}

	return true;
}

bool URequence::SaveUnrealInput(bool Force)
{
	UInputSettings* Settings = GetMutableDefault<UInputSettings>();
	if (!Settings) { return false; }

	if (HasUpdated() || Force)
	{
		//Todo: Store a backup of these mappings - only empty them after the fact when its safe.
		Settings->ActionMappings.Empty();
		Settings->AxisMappings.Empty();

		for (URequenceDevice* d : Devices)
		{
			for (FRequenceInputAction ac : d->Actions)
			{
				if (ac.Key != FKey())
				{
					FInputActionKeyMapping NewAction;
					NewAction.ActionName = FName(*ac.ActionName);
					NewAction.Key = ac.Key;
					NewAction.bShift = ac.bShift;
					NewAction.bCtrl = ac.bCmd;
					NewAction.bAlt = ac.bAlt;
					NewAction.bCmd = ac.bCmd;
					Settings->ActionMappings.Add(NewAction);
				}
			}

			for (FRequenceInputAxis ax : d->Axises)
			{
				if (ax.Key != FKey())
				{
					FInputAxisKeyMapping NewAxis;
					NewAxis.AxisName = FName(*ax.AxisName);
					NewAxis.Key = ax.Key;
					NewAxis.Scale = ax.Scale;
					Settings->AxisMappings.Add(NewAxis);
				}
			}
		}

		Settings->SaveKeyMappings();

		for (TObjectIterator<UPlayerInput> It; It; ++It)
		{
			It->ForceRebuildingKeyMaps(true);
		}
		return true;
	}

	return false;
}

bool URequence::LoadInput(bool ForceDefault)
{
	ClearDevicesAndAxises();

	//If we don't have a save file we didn't change anything, so there's no need to load in the savefile. just roll with the OG!
	URequenceSaveObject* RSO_Instance = Cast<URequenceSaveObject>(UGameplayStatics::CreateSaveGameObject(URequenceSaveObject::StaticClass()));
	if (!UGameplayStatics::DoesSaveGameExist(RSO_Instance->SaveSlotName, RSO_Instance->UserIndex) || ForceDefault)
	{
		if (LoadUnrealInput()) { return SaveInput(); }
	} 
	else
	{
		RSO_Instance = Cast<URequenceSaveObject>(UGameplayStatics::LoadGameFromSlot(RSO_Instance->SaveSlotName, RSO_Instance->UserIndex));
		if (RSO_Instance->Devices.Num() > 0)
		{
			FullAxisList = RSO_Instance->FullAxisList;
			FullActionList = RSO_Instance->FullActionList;
			for (FRequenceSaveObjectDevice SavedDevice : RSO_Instance->Devices)
			{
				URequenceDevice* newDevice = NewObject<URequenceDevice>(this, URequenceDevice::StaticClass());
				newDevice->FromStruct(SavedDevice);
				Devices.Add(newDevice);
			}
			if (Devices.Num() > 0) { return true; }

			//todo: Check for added or removed actions/axies from UnrealInput file.
		}
	}

	return false;
}

bool URequence::SaveInput()
{
	URequenceSaveObject* RSO_Instance = Cast<URequenceSaveObject>(UGameplayStatics::CreateSaveGameObject(URequenceSaveObject::StaticClass()));
	RSO_Instance->FullActionList = FullActionList;
	RSO_Instance->FullAxisList = FullAxisList;
	for (URequenceDevice* Device : Devices)
	{
		RSO_Instance->Devices.Add(Device->ToStruct());
	}
	if (RSO_Instance->Devices.Num() > 0)
	{
		return UGameplayStatics::SaveGameToSlot(RSO_Instance, RSO_Instance->SaveSlotName, RSO_Instance->UserIndex);
	}
	return false;
}

bool URequence::ApplyAxisesAndActions(bool Force)
{
	UInputSettings* Settings = GetMutableDefault<UInputSettings>();
	if (!Settings) { return false; }

	if (HasUpdated() || Force)
	{
		//Todo: Store a backup of these mappings - only empty them after the fact when its safe.
		Settings->ActionMappings.Empty();
		Settings->AxisMappings.Empty();

		for (URequenceDevice* d : Devices)
		{
			for (FRequenceInputAction ac : d->Actions)
			{
				if (ac.Key != FKey())
				{
					FInputActionKeyMapping NewAction;
					NewAction.ActionName = FName(*ac.ActionName);
					NewAction.Key = ac.Key;
					NewAction.bShift = ac.bShift;
					NewAction.bCtrl = ac.bCmd;
					NewAction.bAlt = ac.bAlt;
					NewAction.bCmd = ac.bCmd;
					Settings->ActionMappings.Add(NewAction);
				}
			}

			for (FRequenceInputAxis ax : d->Axises)
			{
				if (ax.Key != FKey())
				{
					FInputAxisKeyMapping NewAxis;
					NewAxis.AxisName = FName(*ax.AxisName);
					NewAxis.Key = ax.Key;
					NewAxis.Scale = ax.Scale;
					Settings->AxisMappings.Add(NewAxis);
				}
			}
		}
		for (TObjectIterator<UPlayerInput> It; It; ++It)
		{
			It->ForceRebuildingKeyMaps(true);
		}
		return true;
	}
	return false;
}

void URequence::OnGameStartup()
{
	if (LoadInput(false)) {
		if (ApplyAxisesAndActions(true)) {
			UE_LOG(LogTemp, Log, TEXT("Requence successfully conducted startup sequence!"));
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("Requence failed to apply axises and actions to runtime on startup!"));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("Requence failed to load input from sav/UE on startup!"));
	return;
}

void URequence::ExportDeviceAsPreset(URequenceDevice* Device)
{
	//Serialize JSON
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Device->GetDeviceAsJson().ToSharedRef(), Writer);

	//Save to file
	FString FileName = FString(Device->DeviceString + "_" + FDateTime::Now().ToString() + ".json");
	bool bAllowOverwriting = true;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.CreateDirectoryTree(*GetDefaultPresetFilePath()))
	{
		FString AbsolutePath = GetDefaultPresetFilePath() + FileName;

		if (bAllowOverwriting || !PlatformFile.FileExists(*AbsolutePath))
		{
			FFileHelper::SaveStringToFile(OutputString, *AbsolutePath);
			UE_LOG(LogTemp, Log, TEXT("Exported %s as preset to %s"), *Device->DeviceString, *AbsolutePath);
		}
	}
}

bool URequence::ImportDeviceAsPreset(FString AbsolutePath)
{
	//Load in our file
	FString InputString;
	if (FFileHelper::LoadFileToString(InputString, *AbsolutePath))
	{
		UE_LOG(LogTemp, Log, TEXT("Requence is trying to import %s"), *AbsolutePath);

		//Deserialize JSON
		TSharedPtr<FJsonObject> JsonDevice;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InputString);

		if (FJsonSerializer::Deserialize(Reader, JsonDevice))
		{
			FString sDeviceType;
			if (JsonDevice->TryGetStringField(TEXT("DeviceType"), sDeviceType))
			{
				URequenceDevice* NewDevice = NewObject<URequenceDevice>(this, URequenceDevice::StaticClass());
				NewDevice->DeviceType = StringToEnum<ERequenceDeviceType>("ERequenceDeviceType", sDeviceType);
				NewDevice->DeviceName = JsonDevice->GetStringField(TEXT("DeviceName"));
				NewDevice->DeviceString = JsonDevice->GetStringField(TEXT("DeviceString"));
				NewDevice->SetJsonAsActions(JsonDevice->GetArrayField(TEXT("Actions")));
				NewDevice->SetJsonAsAxises(JsonDevice->GetArrayField(TEXT("Axises")));
				AddAllEmpty(NewDevice);
				NewDevice->SortAlphabetically();

				//Out with the old, in with the new.
				if (GetDeviceByType(NewDevice->DeviceType))
				{
					Devices.Remove(GetDeviceByType(NewDevice->DeviceType));
				}
				Devices.Add(NewDevice);

				UE_LOG(LogTemp, Log, TEXT("Requence imported %s"), *NewDevice->DeviceName);
				return true;
			}
		}
	}
	return false;
}

TArray<FString> URequence::GetImportableDevicePresets()
{
	IFileManager& fm = IFileManager::Get();
	TArray<FString> toReturn;
	fm.FindFiles(toReturn, *GetDefaultPresetFilePath(), TEXT("json"));
	return toReturn;
}

URequenceDevice* URequence::GetDeviceByString(FString DeviceName)
{
	if (Devices.Num() > 0)
	{
		for (URequenceDevice* Device : Devices)
		{
			if (Device->DeviceString == DeviceName) 
			{
				return Device;
			}
		}
	}

	return nullptr;
}

URequenceDevice* URequence::GetDeviceByType(ERequenceDeviceType DeviceType)
{
	if (Devices.Num() > 0)
	{
		for (URequenceDevice* Device : Devices)
		{
			if (Device->DeviceType == DeviceType)
			{
				return Device;
			}
		}
	}

	return nullptr;
}

URequenceDevice* URequence::CreateDevice(FString KeyName)
{
	ERequenceDeviceType NewDeviceType = URequenceDevice::GetDeviceTypeByKeyString(KeyName);

	if (!IsValid(GetDeviceByType(NewDeviceType)))
	{
		URequenceDevice* device = NewObject<URequenceDevice>(this, URequenceDevice::StaticClass());
		device->DeviceType = NewDeviceType;
		device->DeviceString = URequenceDevice::GetDeviceNameByType(NewDeviceType);
		device->DeviceName = device->DeviceString;
		Devices.Add(device);
		return device;
	}

	return nullptr;
}

void URequence::AddAllEmpty(URequenceDevice* Device)
{
	for (FString ac : FullActionList)
	{
		if (!Device->HasActionBinding(ac, false))
		{
			FRequenceInputAction newAction;
			newAction.ActionName = ac;
			Device->Actions.Add(newAction);
		}
	}
	for (FString ax : FullAxisList)
	{
		if (!Device->HasAxisBinding(ax, false))
		{
			FRequenceInputAxis newAxis;
			newAxis.AxisName = ax;
			Device->Axises.Add(newAxis);
		}
	}
}

void URequence::DebugPrint(bool UseDevices = true)
{
	UE_LOG(LogTemp, Log, TEXT("Requence Debug Print:"));
	if (UseDevices)
	{
		if (Devices.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("- Devices found: %i"), Devices.Num());
			UE_LOG(LogTemp, Log, TEXT("- Actions found: %i"), Actions.Num());
			UE_LOG(LogTemp, Log, TEXT("- Axises found: %i"), Axises.Num());
			for (URequenceDevice* Device : Devices)
			{
				UE_LOG(LogTemp, Log, TEXT("- %s: %s. (Ax: %i, Ac: %i)"), *EnumToString<ERequenceDeviceType>("ERequenceDeviceType", Device->DeviceType), *Device->DeviceString, Device->Axises.Num(), Device->Actions.Num());

				if (Device->Axises.Num() > 0)
				{
					for (FRequenceInputAxis ax : Device->Axises)
					{
						UE_LOG(LogTemp, Log, TEXT("  * Axis %s (%s : %f)"), *ax.AxisName, *ax.Key.ToString(), ax.Scale);
					}
				}
				if (Device->Actions.Num() > 0)
				{
					for (FRequenceInputAction ac : Device->Actions)
					{
						UE_LOG(LogTemp, Log, TEXT("  * Action %s (%s)"), *ac.ActionName, *ac.Key.ToString());
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("- No devices found!"));
		}
	}
	else
	{
		if (Axises.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("- Axises found: %i"), Axises.Num());
			for (FRequenceInputAxis ax : Axises)
			{
				UE_LOG(LogTemp, Log, TEXT("  * Axis %s (%s)"), *ax.AxisName, *ax.Key.ToString());
			}
		}
		else { UE_LOG(LogTemp, Log, TEXT("- No Axises found!")); }
		if (Actions.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("- Actions found: %i"), Actions.Num());
			for (FRequenceInputAction ac : Actions)
			{
				UE_LOG(LogTemp, Log, TEXT("  * Action %s (%s)"), *ac.ActionName, *ac.Key.ToString());
			}
		}
		else { UE_LOG(LogTemp, Log, TEXT("- No Actions found!")); }
	}
}

void URequence::ClearDevicesAndAxises()
{
	Actions.Empty();
	Axises.Empty();
	Devices.Empty();
	FullAxisList.Empty();
	FullActionList.Empty();
}

bool URequence::HasUpdated()
{
	if (Devices.Num() > 0)
	{
		for (URequenceDevice* d : Devices)
		{
			if (d->Updated) { return true; }
		}
	}
	return false;
}