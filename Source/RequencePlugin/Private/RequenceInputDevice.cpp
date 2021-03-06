// Fill out your copyright notice in the Description page of Project Settings.

#include "RequenceInputDevice.h"
#include "Text.h"
#include "Events.h"
#include "SlateApplication.h"
#include "RequenceSaveObject.h"
#include "Requence.h"
#include "RequenceStructs.h"

#define LOCTEXT_NAMESPACE "RequencePlugin"

RequenceInputDevice::RequenceInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) : MessageHandler(InMessageHandler)
{
	InitSDL();
}

RequenceInputDevice::~RequenceInputDevice()
{
	UE_LOG(LogTemp, Log, TEXT("Quitting SDL."));

	SDL_DelEventWatch(HandleSDLEvent, this);

	for (FSDLDeviceInfo Device : Devices) {
		if (Device.Joystick != nullptr) {
			SDL_JoystickClose(Device.Joystick);
		}
	}
	Devices.Empty();

	if (bOwnsSDL)
	{
		SDL_Quit();
	}
}

void RequenceInputDevice::InitSDL()
{
	UE_LOG(LogTemp, Log, TEXT("RequenceSDL starting"));

	if (SDL_WasInit(0) != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SDL already loaded!"));
		bOwnsSDL = false;
	}
	else
	{
		SDL_Init(0);
		bOwnsSDL = true;
		UE_LOG(LogTemp, Log, TEXT("Took ownership of SDL"));
	}

	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Initialized Joystick subsystem"));
	}

	if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Initialized Controller subsystem"));
	}

	for (int i = 0; i < SDL_NumJoysticks(); i++) 
	{
		AddDevice(i);
	}

	LoadRequenceDeviceProperties();

	SDL_AddEventWatch(HandleSDLEvent, this);
}

int RequenceInputDevice::HandleSDLEvent(void* UserData, SDL_Event* Event)
{
	RequenceInputDevice& Self = *static_cast<RequenceInputDevice*>(UserData);

	switch (Event->type) 
	{
		case SDL_JOYDEVICEADDED:
			Self.AddDevice(Event->jdevice.which);
			break;
		case SDL_JOYDEVICEREMOVED:
			Self.RemDevice(Event->jdevice.which);
			break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			Self.HandleInput_Button(Event);
			break;
		case SDL_JOYHATMOTION:
			Self.HandleInput_Hat(Event);
			break;
		case SDL_JOYAXISMOTION:
			Self.HandleInput_Axis(Event);
			break;
		default:
			break;
	}

	return 0;
}

bool RequenceInputDevice::AddDevice(int Which)
{
	if (SDL_IsGameController(Which) == SDL_TRUE) { return false; }

	//It's already in!
	for (FSDLDeviceInfo d : Devices) {
		if (d.Which == Which) { return false; }
	}

	FSDLDeviceInfo Device;
	Device.Which = Which;
	Device.Joystick = SDL_JoystickOpen(Which);
	if (Device.Joystick == nullptr) {
		return false;
	}
	Device.InstanceID = SDL_JoystickInstanceID(Device.Joystick);

	Device.Name = FString(ANSI_TO_TCHAR(SDL_JoystickName(Device.Joystick))).Replace(TEXT("."), TEXT(""), ESearchCase::IgnoreCase);
	UE_LOG(LogTemp, Log, TEXT("Requence input device connected: %s (which: %i, instance: %i)"), *Device.Name, Which, Device.InstanceID);
	UE_LOG(LogTemp, Log, TEXT("- Axises %i"), SDL_JoystickNumAxes(Device.Joystick));
	UE_LOG(LogTemp, Log, TEXT("- Buttons %i"), SDL_JoystickNumButtons(Device.Joystick));
	UE_LOG(LogTemp, Log, TEXT("- Hats %i"), SDL_JoystickNumHats(Device.Joystick));

	//Add Buttons
	for (int i = 0; i < SDL_JoystickNumButtons(Device.Joystick); i++)
	{
		FString keyName = FString::Printf(TEXT("RequenceJoystick_%s_Button_%i"), *Device.Name, i);
		FKey key{ *keyName };
		Device.Buttons.Add(i, key);
		Device.OldButtonState.Add(i, false);

		//Add a new key if this one isn't there yet.
		if (!EKeys::GetKeyDetails(key).IsValid())
		{
			FText textValue = FText::Format(LOCTEXT("DeviceHat", "RequenceJoystick {0} Button {1}"), FText::FromString(Device.Name), FText::AsNumber(i));
			EKeys::AddKey(FKeyDetails(key, textValue, FKeyDetails::GamepadKey));
		}
	}

	//Add Axises
	for (int i = 0; i < SDL_JoystickNumAxes(Device.Joystick); i++)
	{
		FString keyName = FString::Printf(TEXT("RequenceJoystick_%s_Axis_%i"), *Device.Name, i);
		FKey key{ *keyName };
		Device.Axises.Add(i, key);
		Device.OldAxisState.Add(i, 0.f);

		//Add a new key if this one isn't there yet.
		if (!EKeys::GetKeyDetails(key).IsValid())
		{
			FText textValue = FText::Format(LOCTEXT("DeviceHat", "RequenceJoystick {0} Axis {1}"), FText::FromString(Device.Name), FText::AsNumber(i));
			EKeys::AddKey(FKeyDetails(key, textValue, FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
		}
	}

	//Add HATS
	for (int i = 0; i < SDL_JoystickNumHats(Device.Joystick); i++) 
	{
		Device.HatKeys.Add(i, FHatData());
		Device.OldHatState.Add(i, 0);

		//Buttons for all 8 keys
		for (int j = 0; j < _HatDirections.Num(); j++) 
		{
			FString keyName = FString::Printf(TEXT("RequenceJoystick_%s_Hat_%i_%s"), *Device.Name, i, *_HatDirections[j]);
			FKey key{ *keyName };
			Device.HatKeys[i].Buttons.Add(_HatDirectionMap[j], key);

			//Add a new key if this one isn't there yet.
			if (!EKeys::GetKeyDetails(key).IsValid()) 
			{
				FText textValue = FText::Format(LOCTEXT("DeviceHat", "RequenceJoystick {0} Hat {1} {2}"), FText::FromString(Device.Name), FText::AsNumber(i), FText::FromString(_HatDirections[j]));
				EKeys::AddKey(FKeyDetails(key, textValue, FKeyDetails::GamepadKey));
			}
		}
		Device.HatKeys[i].Buttons.Add(SDL_HAT_CENTERED, FKey());

		//Two axises.
		for (int k = 0; k < _HatAxises.Num(); k++)
		{
			FString keyName = FString::Printf(TEXT("RequenceJoystick_%s_Hat_%i_%s-Axis"), *Device.Name, i, *_HatAxises[k]);
			FKey key{ *keyName };
			Device.HatKeys[i].Axises.Add(_HatAxises[k], key);

			//Add a new key if this one isn't there yet.
			if (!EKeys::GetKeyDetails(key).IsValid())
			{
				FText textValue = FText::Format(LOCTEXT("DeviceHat", "RequenceJoystick {0} Hat {1} {2}-Axis"), FText::FromString(Device.Name), FText::AsNumber(i), FText::FromString(_HatAxises[k]));
				EKeys::AddKey(FKeyDetails(key, textValue, FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
			}
		}
	}

	Devices.Add(Device);
	OnDevicesUpdated.Broadcast();
	return true;
}

bool RequenceInputDevice::RemDevice(int InstanceID)
{
	bool found = false;
	for (int i = Devices.Num()-1; i >= 0; i--) {
		if (Devices[i].InstanceID == InstanceID) {
			found = true;
			UE_LOG(LogTemp, Log, TEXT("Requence input device disconnected: %s"), *Devices[i].Name);

			if (Devices[i].Joystick != nullptr)
			{
				SDL_JoystickClose(Devices[i].Joystick);
				Devices[i].Joystick = nullptr;
			}
			else { UE_LOG(LogTemp, Warning, TEXT("Tried to remove %s but the SDL device was a nullpointer! Cleaning up..."), *Devices[i].Name); }

			Devices.RemoveAt(i);
			break;
		}
	}

	//return success.
	OnDevicesUpdated.Broadcast();
	return !found;
}

int RequenceInputDevice::GetDeviceIndexByInstanceID(int InstanceID)
{
	for (int i = 0; i < Devices.Num(); i++)
	{
		if (Devices[i].InstanceID == InstanceID)
		{
			return i;
		}
	}
	return -1;
}


void RequenceInputDevice::LoadRequenceDeviceProperties()
{
	URequenceSaveObject* RSO_Instance = Cast<URequenceSaveObject>(UGameplayStatics::CreateSaveGameObject(URequenceSaveObject::StaticClass()));
	if (!UGameplayStatics::DoesSaveGameExist(RSO_Instance->SaveSlotName, RSO_Instance->UserIndex)) { return; }

	RSO_Instance = Cast<URequenceSaveObject>(UGameplayStatics::LoadGameFromSlot(RSO_Instance->SaveSlotName, RSO_Instance->UserIndex));
	if (RSO_Instance->Devices.Num() <= 0) { return; }

	if (RSO_Instance->RequenceVersion != URequence::Version) { return; }

	DeviceProperties.Empty();
	for (FRequenceSaveObjectDevice SavedDevice : RSO_Instance->Devices)
	{
		if (SavedDevice.DeviceType != ERequenceDeviceType::RDT_Unique) { continue; }
		for (int i = 0; i < SavedDevice.PhysicalAxises.Num(); i++) {
			SavedDevice.PhysicalAxises[i].PrecacheDatapoints();
		}
		DeviceProperties.Add(SavedDevice);
	}
}

void RequenceInputDevice::HandleInput_Hat(SDL_Event* e)
{
	if (!bOwnsSDL) { return; }

	FVector2D HatInput = HatStateToVector(e->jhat.value);
	int DevID = GetDeviceIndexByInstanceID(e->jdevice.which);
	int HatID = e->jhat.hat;

	if (DevID == -1) { return; }
	FVector2D OldHatState = HatStateToVector(Devices[DevID].OldHatState[HatID]);

	//Button
	FKey ButtonKey = Devices[DevID].HatKeys[HatID].Buttons[e->jhat.value];

	if (Devices[DevID].OldHatState.Num() > 0 && _HatDirectionMap.Contains(Devices[DevID].OldHatState[HatID]))
	{
		//Up event for old hat
		FKey OldKey = Devices[DevID].HatKeys[HatID].Buttons[
			Devices[DevID].OldHatState[HatID]
		];
		FKeyEvent UpEvent(OldKey, FSlateApplication::Get().GetModifierKeys(), 0, false, 0, 0);
		FSlateApplication::Get().ProcessKeyUpEvent(UpEvent);
	}

	//down event for new hat, unless SDL_HAT_CENTERED
	if (e->jhat.value != SDL_HAT_CENTERED)
	{
		FKeyEvent DownEvent(ButtonKey, FSlateApplication::Get().GetModifierKeys(), 0, false, 0, 0);
		FSlateApplication::Get().ProcessKeyDownEvent(DownEvent);
	}

	//Axis
	if (OldHatState.X != HatInput.X) 
	{
		FKey XKey = Devices[DevID].HatKeys[HatID].Axises["X"];
		FAnalogInputEvent XEvent(XKey, FSlateApplication::Get().GetModifierKeys(), 0, false, 0, 0, HatInput.X);
		FSlateApplication::Get().ProcessAnalogInputEvent(XEvent);
	}

	if (OldHatState.Y != HatInput.Y) 
	{
		FKey YKey = Devices[DevID].HatKeys[HatID].Axises["Y"];
		FAnalogInputEvent YEvent(YKey, FSlateApplication::Get().GetModifierKeys(), 0, false, 0, 0, HatInput.Y);
		FSlateApplication::Get().ProcessAnalogInputEvent(YEvent);
	}

	Devices[DevID].OldHatState[e->jhat.hat] = e->jhat.value;
}

void RequenceInputDevice::HandleInput_Button(SDL_Event* e)
{
	if (!bOwnsSDL) { return; }

	int DevID = GetDeviceIndexByInstanceID(e->jdevice.which);
	int ButtonID = e->jbutton.button;
	bool NewButtonState = (e->jbutton.state > 0) ? true : false;

	if (DevID == -1) { return; }
	if (!Devices[DevID].Buttons.Contains(ButtonID)) { return; }

	if (NewButtonState)
	{
		//Down event
		FKeyEvent DownEvent(Devices[DevID].Buttons[ButtonID], 
			FSlateApplication::Get().GetModifierKeys(), 0, false, 0, 0);
		FSlateApplication::Get().ProcessKeyDownEvent(DownEvent);
	}
	else
	{
		//Up Event
		FKeyEvent UpEvent(Devices[DevID].Buttons[ButtonID], 
			FSlateApplication::Get().GetModifierKeys(), 0, false, 0, 0);
		FSlateApplication::Get().ProcessKeyUpEvent(UpEvent);
	}

	Devices[DevID].OldButtonState[ButtonID] = NewButtonState;
}

void RequenceInputDevice::HandleInput_Axis(SDL_Event* e)
{
	if (!bOwnsSDL) { return; }

	int DevID = GetDeviceIndexByInstanceID(e->jdevice.which);
	int AxisID = e->jaxis.axis;
	float NewAxisState = FMath::Clamp(e->jaxis.value / (e->jaxis.value < 0 ? 32768.0f : 32767.0f), -1.f, 1.f);

	if (DevID == -1) { return; }

	//Filter based on Requence save file.
	if (DeviceProperties.Num() > 0) {
		for (int i = 0; i < DeviceProperties.Num(); i++)
		{
			//Check for the correct device, and if it has physicalAxis data stored.
			if (DeviceProperties[i].DeviceString != Devices[DevID].Name) { continue; }
			if (DeviceProperties[i].PhysicalAxises.Num() <= 0) { continue; }

			switch (DeviceProperties[i].PhysicalAxises[AxisID].InputRange) {
			case ERequencePAInputRange::RPAIR_Halved:
				//Compress -1~1 to 0~1
				NewAxisState = FMath::Clamp((NewAxisState + 1) / 2, 0.f, 1.f);
				break;
			case ERequencePAInputRange::RPAIR_HalvedNegative:
				//Compress -1~1 to -1~0
				NewAxisState = FMath::Clamp((NewAxisState - 1) / 2, -1.f, 0.f);
				break;
			default:
				break;
			}

			//If we have datapoints and they are precached, interpolate data.
			if (DeviceProperties[i].PhysicalAxises[AxisID].DataPoints.Num() <= 0 && DeviceProperties[i].PhysicalAxises[AxisID].bIsPrecached) { continue; }
			float interp = URequenceStructs::Interpolate(DeviceProperties[i].PhysicalAxises[AxisID].DataPoints, NewAxisState);
			NewAxisState = interp;
		}
	}

	if (!Devices[DevID].Axises.Contains(AxisID)) { return; }

	FAnalogInputEvent AxisEvent(Devices[DevID].Axises[AxisID], FSlateApplication::Get().GetModifierKeys(), 0, false, 0, 0, NewAxisState);
	FSlateApplication::Get().ProcessAnalogInputEvent(AxisEvent);

	Devices[DevID].OldAxisState[AxisID] = NewAxisState;
}

FVector2D RequenceInputDevice::HatStateToVector(uint8 SDL_HAT_STATE)
{
	FVector2D HatInput;

	switch (SDL_HAT_STATE)
	{
		case SDL_HAT_CENTERED:
			HatInput = FVector2D::ZeroVector;
			break;
		case SDL_HAT_DOWN:
			HatInput.Y = -1;
			HatInput.X = 0;
			break;
		case SDL_HAT_LEFTDOWN:
			HatInput.Y = -1;
			HatInput.X = -1;
			break;
		case SDL_HAT_LEFT:
			HatInput.Y = 0;
			HatInput.X = -1;
			break;
		case SDL_HAT_LEFTUP:
			HatInput.Y = 1;
			HatInput.X = -1;
			break;
		case SDL_HAT_UP:
			HatInput.Y = 1;
			HatInput.X = 0;
			break;
		case SDL_HAT_RIGHTUP:
			HatInput.Y = 1;
			HatInput.X = 1;
			break;
		case SDL_HAT_RIGHT:
			HatInput.Y = 0;
			HatInput.X = 1;
			break;
		case SDL_HAT_RIGHTDOWN:
			HatInput.Y = -1;
			HatInput.X = 1;
			break;
	}

	return HatInput;
}

void RequenceInputDevice::Tick(float DeltaTime)
{

}

void RequenceInputDevice::SendControllerEvents()
{
	if (bOwnsSDL)
	{
		SDL_Event Event;
		while (SDL_PollEvent(&Event)) {}
	}
}

#undef LOCTEXT_NAMESPACE
