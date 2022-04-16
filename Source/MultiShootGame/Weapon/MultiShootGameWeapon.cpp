// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiShootGameWeapon.h"
#include "BulletShell.h"
#include "MultiShootGameProjectile.h"
#include "MultiShootGame/Character/MultiShootGameCharacter.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "MultiShootGame/SaveGame/ChooseWeaponSaveGame.h"
#include "Particles/ParticleSystemComponent.h"

// Sets default values
AMultiShootGameWeapon::AMultiShootGameWeapon()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	WeaponMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponmeshComponent"));
	WeaponMeshComponent->SetupAttachment(RootComponent);
	WeaponMeshComponent->SetCastHiddenShadow(true);

	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
	AudioComponent->SetAutoActivate(false);
}

// Called when the game starts or when spawned
void AMultiShootGameWeapon::BeginPlay()
{
	Super::BeginPlay();

	const UChooseWeaponSaveGame* SaveGame = Cast<UChooseWeaponSaveGame>(
		UGameplayStatics::LoadGameFromSlot(TEXT("ChooseWeapon"), 0));
	if (SaveGame)
	{
		TArray<FWeaponInfo> WeaponInfoList;
		FWeaponInfo WeaponInfo;
		switch (CurrentWeaponMode)
		{
		case EWeaponMode::Weapon:
			WeaponInfoList = SaveGame->MainWeaponList;
			WeaponInfo = WeaponInfoList[SaveGame->MainWeaponIndex];
			break;
		case EWeaponMode::Sniper:
			WeaponInfoList = SaveGame->SecondWeaponList;
			WeaponInfo = WeaponInfoList[SaveGame->SecondWeaponIndex];
			break;
		case EWeaponMode::Shotgun:
			WeaponInfoList = SaveGame->ShotgunWeaponList;
			WeaponInfo = WeaponInfoList[SaveGame->ShotgunWeaponIndex];
			break;
		}
		WeaponMeshComponent->SetSkeletalMesh(WeaponInfo.WeaponMesh);
		BaseDamage = WeaponInfo.BaseDamage;
		RateOfFire = WeaponInfo.RateOfFire;
		DelayOfShotgun = WeaponInfo.DelayOfShotgun;
		BulletSpread = WeaponInfo.BulletSpread;
		CameraSpread = WeaponInfo.CameraSpread;
		AimVector = WeaponInfo.AimVector;
		AimTexture = WeaponInfo.AimTexture;
	}

	TimeBetweenShots = 60.0f / RateOfFire;
}

void AMultiShootGameWeapon::ShakeCamera()
{
	AMultiShootGameCharacter* MyOwner = Cast<AMultiShootGameCharacter>(GetOwner());
	if (MyOwner)
	{
		if (MyOwner->GetWeaponMode() == EWeaponMode::Weapon)
		{
			MyOwner->AddControllerYawInput(FMath::RandRange(-1 * CameraSpread, CameraSpread));
			MyOwner->AddControllerPitchInput(-1 * FMath::RandRange(0.f, CameraSpread));
		}

		APlayerController* PlayerController = Cast<APlayerController>(MyOwner->GetController());
		if (PlayerController)
		{
			PlayerController->ClientPlayCameraShake(FireCameraShake);
		}
	}
}

void AMultiShootGameWeapon::Fire()
{
	AMultiShootGameCharacter* MyOwner = Cast<AMultiShootGameCharacter>(GetOwner());
	if (MyOwner)
	{
		FVector EyeLocation;
		FRotator EyeRotation;

		if (MyOwner->GetAimed())
		{
			EyeLocation = MyOwner->GetCurrentFPSCamera()->GetCameraComponent()->GetComponentLocation();
			EyeRotation = MyOwner->GetCurrentFPSCamera()->GetCameraComponent()->GetComponentRotation();
		}
		else
		{
			EyeLocation = MyOwner->GetCameraComponent()->GetComponentLocation();
			EyeRotation = MyOwner->GetCameraComponent()->GetComponentRotation();
		}

		FVector ShotDirection = EyeRotation.Vector();

		const float HalfRad = FMath::DegreesToRadians(BulletSpread);
		ShotDirection = FMath::VRandCone(ShotDirection, HalfRad, HalfRad);

		const FVector TraceEnd = EyeLocation + (ShotDirection * 3000.f);

		if (Cast<AMultiShootGameFPSCamera>(this))
		{
			const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(EyeLocation, TraceEnd);

			const FRotator TargetRotation = FRotator(0, LookAtRotation.Yaw - 90.f, LookAtRotation.Pitch * -1.f);

			Cast<AMultiShootGameCharacter>(GetOwner())->GetFPSCameraSceneComponent()->SetWorldRotation(TargetRotation);
		}

		PlayFireEffect(TraceEnd);

		AudioComponent->Play();

		LastFireTime = GetWorld()->TimeSeconds;
	}
}

void AMultiShootGameWeapon::PlayFireEffect(FVector TraceEndPoint)
{
	if (MuzzleEffect)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, WeaponMeshComponent, MuzzleSocketName);
	}

	if (TracerEffectClass)
	{
		const FVector MuzzleLocation = WeaponMeshComponent->GetSocketLocation(MuzzleSocketName);
		const FRotator ShotDirection = UKismetMathLibrary::FindLookAtRotation(MuzzleLocation, TraceEndPoint);

		AMultiShootGameProjectile* Projectile = Cast<AMultiShootGameProjectile>(
			GetWorld()->SpawnActor<AActor>(TracerEffectClass, MuzzleLocation, ShotDirection));
		Projectile->SetOwner(this);
		Projectile->BaseDamage = BaseDamage;
	}

	if (BulletShellClass)
	{
		const FVector BulletShellLocation = WeaponMeshComponent->GetSocketLocation(BulletShellName);
		const FRotator BulletShellRotation = WeaponMeshComponent->GetComponentRotation();

		ABulletShell* BulletShell = Cast<ABulletShell>(
			GetWorld()->SpawnActor<AActor>(BulletShellClass, BulletShellLocation, BulletShellRotation));
		BulletShell->SetOwner(this);
		BulletShell->ThrowBulletShell();
	}

	ShakeCamera();
}

void AMultiShootGameWeapon::StartFire()
{
	const float FirstDelay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);

	GetWorldTimerManager().SetTimer(TimerHandle, this, &AMultiShootGameWeapon::Fire, TimeBetweenShots, true,
	                                FirstDelay);
}

void AMultiShootGameWeapon::StopFire(bool stopAudio)
{
	GetWorldTimerManager().ClearTimer(TimerHandle);

	if (stopAudio)
	{
		AudioComponent->Stop();
	}
}

void AMultiShootGameWeapon::ShotgunFire()
{
	if (LastFireTime == 0)
	{
		Fire();

		return;
	}

	const float CurrentFireTime = GetWorld()->GetTimeSeconds();

	if (CurrentFireTime - LastFireTime > DelayOfShotgun)
	{
		Fire();
	}
}

void AMultiShootGameWeapon::EnablePhysicsSimulate()
{
	WeaponMeshComponent->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	WeaponMeshComponent->SetCollisionProfileName("BlockAll");
	WeaponMeshComponent->SetSimulatePhysics(true);
}

void AMultiShootGameWeapon::ReloadShowClip(bool Enabled)
{
	if (Enabled)
	{
		WeaponMeshComponent->UnHideBoneByName(ClipBoneName);
	}
	else
	{
		WeaponMeshComponent->HideBoneByName(ClipBoneName, PBO_None);
	}
}

UAudioComponent* AMultiShootGameWeapon::GetAudioComponent() const
{
	return AudioComponent;
}

USkeletalMeshComponent* AMultiShootGameWeapon::GetWeaponMeshComponent() const
{
	return WeaponMeshComponent;
}
