// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiShootGameWeapon.h"

#include "MultiShootGameProjectile.h"
#include "../Character/MultiShootGameCharacter.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Particles/ParticleSystemComponent.h"

// Sets default values
AMultiShootGameWeapon::AMultiShootGameWeapon()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	WeaponMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponmeshComponent"));
	WeaponMeshComponent->SetupAttachment(RootComponent);

	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
	AudioComponent->SetAutoActivate(false);
}

// Called when the game starts or when spawned
void AMultiShootGameWeapon::BeginPlay()
{
	Super::BeginPlay();

	TimeBetweenShots = 60.0f / RateOfFire;
}

void AMultiShootGameWeapon::Fire()
{
	AMultiShootGameCharacter* MyOwner = Cast<AMultiShootGameCharacter>(GetOwner());
	if (MyOwner)
	{
		FVector EyeLocation;
		FRotator EyeRotation;

		if (MyOwner->bAimed)
		{
			EyeLocation = MyOwner->CurrentFPSCamera->CameraComponent->GetComponentLocation();
			EyeRotation = MyOwner->CurrentFPSCamera->CameraComponent->GetComponentRotation();
		}
		else
		{
			EyeLocation = MyOwner->CameraComponent->GetComponentLocation();
			EyeRotation = MyOwner->CameraComponent->GetComponentRotation();
		}

		FVector ShotDirection = EyeRotation.Vector();

		const float HalfRad = FMath::DegreesToRadians(BulletSpread);
		ShotDirection = FMath::VRandCone(ShotDirection, HalfRad, HalfRad);

		FVector TraceEnd = EyeLocation + (ShotDirection * 3000.f);

		if (Cast<AMultiShootGameFPSCamera>(this))
		{
			const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(EyeLocation, TraceEnd);

			const FRotator TargetRotation = FRotator(0, LookAtRotation.Yaw - 90.f, LookAtRotation.Pitch * -1.f);

			Cast<AMultiShootGameCharacter>(GetOwner())->FPSCameraSceneComponent->SetWorldRotation(TargetRotation);
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
	}

	AMultiShootGameCharacter* MyOwner = Cast<AMultiShootGameCharacter>(GetOwner());
	if (MyOwner)
	{
		APlayerController* PlayerController = Cast<APlayerController>(MyOwner->GetController());
		if (PlayerController)
		{
			PlayerController->ClientPlayCameraShake(FireCameraShake);
		}
	}
}

void AMultiShootGameWeapon::StartFire()
{
	const float FirstDelay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);

	GetWorldTimerManager().SetTimer(TimerHandle, this, &AMultiShootGameWeapon::Fire, TimeBetweenShots, true,
	                                FirstDelay);
}

void AMultiShootGameWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(TimerHandle);

	if (Cast<AMultiShootGameCharacter>(GetOwner())->WeaponMode == EWeaponMode::Weapon)
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

	float CurrentFireTime = GetWorld()->GetTimeSeconds();

	if (CurrentFireTime - LastFireTime > DelayOfShotgun)
	{
		Fire();
	}
}
