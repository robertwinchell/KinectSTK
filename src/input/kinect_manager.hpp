#ifndef KINECT_MANAGER_HPP
#define KINECT_MANAGER_HPP


#include "input/kinect.hpp"
#include "states_screens/dialogs/message_dialog.hpp"
#include "utils/cpp2011.h"
#include "IEventReceiver.h"
#include <pthread.h>


#define MAX_KINECTS 1


struct kinect_t;
class GamepadConfig;
class GamePadDevice;
class KinectManager;

extern KinectManager* kinect_manager;

class KinectManager
{
private:
	Kinect     *m_kinect;
	int        m_number_kinects;

#define KINECT_THREADING
#ifdef KINECT_THREADING
    pthread_t       m_thread;
	bool            m_shut;
#endif
	static bool     m_enabled;

    
    void threadFunc();
    static void* threadFuncWrapper(void* data);
    void setKinectBindings(GamepadConfig* gamepad_config);

public:
	enum KINECT_MOTION
	{
		STEER_LEFT,
		STEER_RIGHT,
		ACCELERATE,
		BRAKE
	};

	KinectManager(void);
	~KinectManager(void);
	int KinectManager::GetNumberOfKinects();
	static void enable() { m_enabled = true; }
    static bool  isEnabled() { return m_enabled; }

    void launchDetection(int timeout);
    void update();
    void cleanup();

    int askUserToConnectKinect();
    
	class KinectDialogListener : public MessageDialog::IConfirmDialogListener
    {
    public:
        virtual void onConfirm() OVERRIDE;
    };
};



#endif
