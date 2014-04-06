#include "kinect_manager.hpp"
#include "graphics/irr_driver.hpp"
#include "guiengine/modaldialog.hpp"
#include "input/input_manager.hpp"
#include "input/device_manager.hpp"
#include "input/kinect.hpp"
#include "utils/string_utils.hpp"
#include "utils/time.hpp"
#include "utils/translation.hpp"

KinectManager *kinect_manager;

bool KinectManager::m_enabled = false;

static const int    KINECT_START_IRR_ID   = 32;

KinectManager::KinectManager(void)
{
	m_kinect = NULL;
	#ifdef KINECT_THREADING
		m_shut = false;
	#endif
}


KinectManager::~KinectManager(void)
{
	cleanup();
}

void KinectManager::launchDetection(int timeout)
{
    cleanup();

	int nb_found_kinects = m_kinect->GetNumberofKinects();
	if(nb_found_kinects == 0)
		return;
	
	DeviceManager* device_manager = input_manager->getDeviceList();
    GamepadConfig* gamepad_config = NULL;

    device_manager->getConfigForGamepad(KINECT_START_IRR_ID, "Kinect",
                                        &gamepad_config);
    int num_buttons = 4;
    gamepad_config->setNumberOfButtons(num_buttons);
    gamepad_config->setNumberOfAxis(1);
	

	

	#ifdef KINECT_THREADING
		m_shut = false;
		pthread_create(&m_thread, NULL, &threadFuncWrapper, this->m_kinect);
	#endif

   
}   



void KinectManager::setKinectBindings(GamepadConfig* gamepad_config)
{
	gamepad_config->setBinding(PA_STEER_LEFT,   Input::IT_KINECTMOTION, KINECT_MOTION::STEER_LEFT);
	gamepad_config->setBinding(PA_STEER_RIGHT,  Input::IT_KINECTMOTION, KINECT_MOTION::STEER_RIGHT);
	gamepad_config->setBinding(PA_ACCEL,        Input::IT_KINECTMOTION, KINECT_MOTION::ACCELERATE);
	gamepad_config->setBinding(PA_BRAKE,        Input::IT_KINECTMOTION, KINECT_MOTION::BRAKE);

	
} 


void KinectManager::cleanup()
{
	if(m_number_kinects>0)
    {
        DeviceManager* device_manager = input_manager->getDeviceList();

        GamePadDevice* first_gamepad_device =
                     device_manager->getGamePadFromIrrID(KINECT_START_IRR_ID);
        assert(first_gamepad_device);

        DeviceConfig*  gamepad_config =
                                      first_gamepad_device->getConfiguration();
        assert(gamepad_config);
		device_manager->deleteConfig(gamepad_config);

        
#ifdef KINECT_THREADING
        m_shut = true;
        pthread_join(m_thread, NULL);
#endif
        
    }

	delete m_kinect;
    
    m_kinect = NULL;
#ifdef KINECT_THREADING
    m_shut                = false;
#endif
}  

// ----------------------------------------------------------------------------
void KinectManager::update()
{
#ifndef KINECTMOTE_THREADING
    threadFunc();
#endif
	irr::SEvent event = m_kinect->getIrrEvent();
    input_manager->input(event);
}   

void KinectManager::threadFunc()
{

}   

void* KinectManager::threadFuncWrapper(void *data)
{
	((Kinect*)data)->UpdateSkeleton();
    return NULL;
}   


int KinectManager::askUserToConnectKinect()
{
    new MessageDialog(
#ifdef WIN32
        _("Connect your Kinect to the usb"),
#else
        _("Connect your kinect to the usb"),
#endif
        MessageDialog::MESSAGE_DIALOG_OK_CANCEL,
        new KinectDialogListener(), true);

	return m_number_kinects;
}   

int KinectManager::GetNumberOfKinects()
{
	return m_kinect->GetNumberofKinects();
}

void KinectManager::KinectDialogListener::onConfirm()
{
    GUIEngine::ModalDialog::dismiss();

    kinect_manager->launchDetection(5);

    int nb_kinects = kinect_manager->GetNumberOfKinects();
    if(nb_kinects > 0)
    {
        core::stringw msg = StringUtils::insertValues(
            _("Found %d Kinect(s)"),
            core::stringw(nb_kinects));

        new MessageDialog( msg );

    }
    else
    {
        new MessageDialog( _("Could not detect any wiimote :/") );
    }
}
