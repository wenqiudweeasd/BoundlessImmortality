#pragma once
#include "CoreMinimal.h"

/**
 * PlatformBridge of GDD 10.3: gameplay never calls wx directly. The WeChat adapter belongs to the mini-game host and
 * cannot exist until the port runs on devices; outside WeChat the local mock answers, clearly labelled as such.
 */
struct FWanJianAdResult
{
    bool bCompleted=false;
    /** A mock answer proves nothing about a real view, so a release build must refuse it. */
    bool bMock=true;
};

class IWanJianPlatform
{
public:
    virtual ~IWanJianPlatform()=default;
    virtual FString Login()=0;
    virtual FWanJianAdResult ShowRewardedAd()=0;
    /** Returns the room token a share card would carry. Sharing alone never changes rewards or stage state. */
    virtual FString ShareStage(int32 StageId)=0;
};

class BOUNDLESSIMMORTALITY_API FWanJianLocalPlatform final:public IWanJianPlatform
{
public:
    virtual FString Login()override{return TEXT("local-player");}
    virtual FWanJianAdResult ShowRewardedAd()override{return {bAdCompletes,true};}
    virtual FString ShareStage(int32 StageId)override{return FString::Printf(TEXT("local-room-%d"),StageId);}
    bool bAdCompletes=true;
};
