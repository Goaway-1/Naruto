#include "NPlayer.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NCameraManager.h"
#include "NGameMode.h"
#include "NWeapon.h"
#include "Net/UnrealNetwork.h"

ANPlayer::ANPlayer() {
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	/** Configure character Rotation */
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationPitch = false;

	/** Configure character movement */
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 2000.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 800.f;

	/* Jump Setting */
	JumpMaxCount = 2;
	JumpMovementForce = 1000.f;
	GetCharacterMovement()->JumpZVelocity = 1400.f;
	GetCharacterMovement()->AirControl = 0.f;
	GetCharacterMovement()->AirControlBoostMultiplier = 0.f;
	GetCharacterMovement()->AirControlBoostVelocityThreshold = 0.f;

	/* Impressed Keys */
	SetKeyUpDown(EKeyUpDown::EKUD_Default);
	SetKeyLeftRight(EKeyLeftRight::EKLR_Default);

	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;

	/* Weapon */
	WeaponAttachSocketName = "WeaponSocket";
}
void ANPlayer::BeginPlay() {
	Super::BeginPlay();
	
	/** Find MainCameraManager & Set */
	if (CameraManagerClass) {
		ANCameraManager* TargetCamera = Cast<ANCameraManager>(UGameplayStatics::GetActorOfClass(this, ANCameraManager::StaticClass()));

		if (TargetCamera) {
			CameraManager = TargetCamera;
			APlayerController* ControllComp = Cast<APlayerController>(GetController());
			if (ControllComp) {
				PlayerControlComp = ControllComp;
				PlayerControlComp->bAutoManageActiveCameraTarget = false;
				PlayerControlComp->ClientSetViewTarget(CameraManager);
			}
		}
		else GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Error! MainCameraManager does not exist in the world!"));
	}

	if (CameraManager) CameraManager->SetPlayer(this);

	/** Weapon */
	if (HasAuthority()) {
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (StarterWeaponClass) CurrentWeapon = GetWorld()->SpawnActor<ANWeapon>(StarterWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (CurrentWeapon) {
			CurrentWeapon->SetOwner(this);
			CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocketName);
		}
	}
}
void ANPlayer::PossessedBy(AController* NewController) {
	Super::PossessedBy(NewController);
}
void ANPlayer::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	/** Setting the rotation of the controller according to the rotation of the CameraManager */
	if (CameraManager && HasAuthority()) {
		FRotator NRot = FRotator(CameraManager->GetActorRotation().Pitch, CameraManager->GetActorRotation().Yaw, GetController()->GetControlRotation().Roll);
		APlayerController* PController = Cast<APlayerController>(GetController());
		if (PController) {
			PController->ClientSetRotation(NRot);
		}
	}
}
void ANPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed,this, &ANPlayer::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released,this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &ANPlayer::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ANPlayer::MoveRight);
}
void ANPlayer::MoveForward(float Value) {
	FRotator Rot = FRotator(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(UKismetMathLibrary::GetForwardVector(Rot), Value);

	if (Value > 0) SetKeyUpDown(EKeyUpDown::EKUD_Up);
	else if (Value < 0) SetKeyUpDown(EKeyUpDown::EKUD_Down);
	else SetKeyUpDown(EKeyUpDown::EKUD_Default);
}
void ANPlayer::MoveRight(float Value) {
	FRotator Rot = FRotator(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(UKismetMathLibrary::GetRightVector(Rot), Value);

	if (Value > 0) SetKeyLeftRight(EKeyLeftRight::EKLR_Right);
	else if (Value < 0) SetKeyLeftRight(EKeyLeftRight::EKLR_Left);
	else SetKeyLeftRight(EKeyLeftRight::EKLR_Default);
}
void ANPlayer::Jump() {
	NJump();
}
void ANPlayer::NJump() {
	if (!HasAuthority()) {
		NServerJump();
		return;
	}

	if (JumpCurrentCount++ < 2) {
		UE_LOG(LogTemp, Warning, TEXT("LOCAL JUMP : %d"), JumpCurrentCount);
		FVector ForceVec = (GetActorUpVector() * (JumpMovementForce - GetVelocity().Size())) + (GetActorForwardVector() * (JumpMovementForce - GetVelocity().Size()));
		ForceVec.Z = 1400.f;
		LaunchCharacter(ForceVec, false, false);
	}
}
void ANPlayer::NServerJump_Implementation() {
	//NJump();
	if (++JumpCurrentCount < 2) {
		UE_LOG(LogTemp, Warning, TEXT("SERVER JUMP : %d"), JumpCurrentCount);
		FVector ForceVec = (GetActorUpVector() * (JumpMovementForce - GetVelocity().Size())) + (GetActorForwardVector() * (JumpMovementForce - GetVelocity().Size()));
		ForceVec.Z = 1400.f;
		LaunchCharacter(ForceVec, false, false);
	}
}
bool ANPlayer::NServerJump_Validate() {
	return true;
}
void ANPlayer::ResetJumpState() {
	Super::ResetJumpState();
}
//void ANPlayer::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const {
//	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
//
//	DOREPLIFETIME(ANPlayer, CurrentWeapon);
//}