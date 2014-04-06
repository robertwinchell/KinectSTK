#include "kinect.hpp"


Kinect::Kinect(void)
{
	HRESULT hr = NuiCreateSensorByIndex(0,  &m_kinect_handle);
    if (SUCCEEDED(hr))
    {
		
		m_kinect_handle->NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON);
		m_hNextSkeletonEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
		hr = m_kinect_handle->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE | NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT);
	}
}

void Kinect::resetIrrEvent()
{
    irr::SEvent &event = m_irr_event.getData();
	event.EventType = irr::EET_KINECT_INPUT_EVENT;
	event.KinectEvent.isLeftFootUp = false;
	event.KinectEvent.isRightFootUp = false;
	event.KinectEvent.isSteeringLeft = false;
	event.KinectEvent.isSteeringRight = false;

}

void Kinect::UpdateSkeleton()
{
	 if ( WAIT_OBJECT_0 == WaitForSingleObject(m_hNextSkeletonEvent, 0) )
      {
		NUI_SKELETON_FRAME skeletonFrame = {0};
		HRESULT hr = m_kinect_handle->NuiSkeletonGetNextFrame(0, &skeletonFrame);
        if (SUCCEEDED(hr))
        {
			const NUI_SKELETON_DATA & skeleton = skeletonFrame.SkeletonData[0];
			leftHand = skeleton.SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
			rightHand = skeleton.SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
			leftFoot =  skeleton.SkeletonPositions[NUI_SKELETON_POSITION_FOOT_LEFT];
			rightFoot = skeleton.SkeletonPositions[NUI_SKELETON_POSITION_FOOT_RIGHT];

		m_irr_event.lock();
		{
			irr::SEvent::SKinectInput &ev = m_irr_event.getData().KinectEvent;
			ev.isLeftFootUp = false;
			ev.isRightFootUp = false;
			ev.isSteeringLeft = false;
			ev.isSteeringRight = false;			
		}
		m_irr_event.unlock();
        }
      }
}

int Kinect::GetNumberofKinects()
{
	int nb_found_kinects = 0;
	NuiGetSensorCount(&nb_found_kinects);
	return nb_found_kinects;
}

irr::SEvent Kinect::getIrrEvent()
{
    return m_irr_event.getAtomic();
}

Kinect::~Kinect(void)
{

}
