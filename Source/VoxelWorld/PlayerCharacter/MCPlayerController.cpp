#include "MCPlayerController.h"
#include "MCPlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Blueprint/UserWidget.h"

AMCPlayerController::AMCPlayerController()
{
}

void AMCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

	if (HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}
}

void AMCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMCPlayerController::Move);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMCPlayerController::Look);
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AMCPlayerController::StartJump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMCPlayerController::StopJump);
		EIC->BindAction(UseAction, ETriggerEvent::Triggered, this, &AMCPlayerController::UseTriggered);
		EIC->BindAction(UseAction, ETriggerEvent::Completed, this, &AMCPlayerController::UseStopped);
		EIC->BindAction(SecondUseAction, ETriggerEvent::Started, this, &AMCPlayerController::SecondUse);
		EIC->BindAction(Slot1Action, ETriggerEvent::Started, this, &AMCPlayerController::SelectSlot1);
		EIC->BindAction(Slot2Action, ETriggerEvent::Started, this, &AMCPlayerController::SelectSlot2);
		EIC->BindAction(Slot3Action, ETriggerEvent::Started, this, &AMCPlayerController::SelectSlot3);
		EIC->BindAction(Slot4Action, ETriggerEvent::Started, this, &AMCPlayerController::SelectSlot4);
		EIC->BindAction(MenuAction, ETriggerEvent::Started, this, &AMCPlayerController::ToggleMenu);
	}
}

void AMCPlayerController::Move(const FInputActionValue& Value)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	const FVector2D Input = Value.Get<FVector2D>();
	const FRotator YawRotation(0.0, ControlRotation.Yaw, 0.0);

	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	ControlledPawn->AddMovementInput(ForwardDir, Input.Y);
	ControlledPawn->AddMovementInput(RightDir, Input.X);
}

void AMCPlayerController::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddYawInput(Input.X);
	AddPitchInput(Input.Y);
}

void AMCPlayerController::StartJump()
{
	if (ACharacter* Char = Cast<ACharacter>(GetPawn()))
	{
		Char->Jump();
	}
}

void AMCPlayerController::StopJump()
{
	if (ACharacter* Char = Cast<ACharacter>(GetPawn()))
	{
		Char->StopJumping();
	}
}

void AMCPlayerController::UseTriggered()
{
	if (AMCPlayerCharacter* Char = Cast<AMCPlayerCharacter>(GetPawn()))
	{
		Char->OnUseTriggered();
	}
}

void AMCPlayerController::UseStopped()
{
	if (AMCPlayerCharacter* Char = Cast<AMCPlayerCharacter>(GetPawn()))
	{
		Char->OnUseCompleted();
	}
}

void AMCPlayerController::SecondUse()
{
	if (AMCPlayerCharacter* Char = Cast<AMCPlayerCharacter>(GetPawn()))
	{
		Char->PlaceSelectedBlock();
	}
}

void AMCPlayerController::SelectSlot1()
{
	if (AMCPlayerCharacter* Char = Cast<AMCPlayerCharacter>(GetPawn()))
	{
		Char->SelectSlot(0);
	}
}

void AMCPlayerController::SelectSlot2()
{
	if (AMCPlayerCharacter* Char = Cast<AMCPlayerCharacter>(GetPawn()))
	{
		Char->SelectSlot(1);
	}
}

void AMCPlayerController::SelectSlot3()
{
	if (AMCPlayerCharacter* Char = Cast<AMCPlayerCharacter>(GetPawn()))
	{
		Char->SelectSlot(2);
	}
}

void AMCPlayerController::SelectSlot4()
{
	if (AMCPlayerCharacter* Char = Cast<AMCPlayerCharacter>(GetPawn()))
	{
		Char->SelectSlot(3);
	}
}

void AMCPlayerController::ToggleMenu()
{
	if (IsMenuOpen())
	{
		HideMenu();
	}
	else
	{
		ShowMenu();
	}
}

void AMCPlayerController::ShowMenu()
{
	if (!MenuWidgetClass)
	{
		return;
	}

	if (!MenuWidget)
	{
		MenuWidget = CreateWidget<UUserWidget>(this, MenuWidgetClass);
	}

	if (MenuWidget && !MenuWidget->IsInViewport())
	{
		MenuWidget->AddToViewport(10);
		SetInputMode(FInputModeGameAndUI());
		bShowMouseCursor = true;
		SetPause(true);
	}
}

void AMCPlayerController::HideMenu()
{
	if (MenuWidget && MenuWidget->IsInViewport())
	{
		MenuWidget->RemoveFromParent();
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	SetPause(false);
}

bool AMCPlayerController::IsMenuOpen() const
{
	return MenuWidget && MenuWidget->IsInViewport();
}
