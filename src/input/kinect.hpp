#ifndef KINECT_HPP
#define KINECT_HPP

#include "utils/synchronised.hpp"
#include "IEventReceiver.h"
#include "NuiApi.h"
#include "NuiSkeleton.h"


class Kinect
{
private:
	INuiSensor* m_kinect_handle;
	NUI_SKELETON_FRAME *m_skeleton_frame;
    HANDLE m_hNextSkeletonEvent;
	Vector4 leftHand;
	Vector4 rightHand;
	Vector4 leftFoot;
	Vector4 rightFoot;
	Synchronised<irr::SEvent> m_irr_event;
	void Kinect::resetIrrEvent();
public:
	Kinect(void);
	void Kinect::UpdateSkeleton();
	int Kinect::GetNumberofKinects();
	irr::SEvent Kinect::getIrrEvent();
	~Kinect(void);
};

#endif
