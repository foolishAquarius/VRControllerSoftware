#include "stdafx.h"

#include "Core/CServerDriver.h"
#include "Core/CLeapPoller.h"
#include "Devices/CLeapController/CLeapControllerIndex.h"
#include "Devices/CLeapStation.h"

#include "Core/CDriverConfig.h"
#include "Utils/Utils.h"

extern char g_modulePath[];

const std::vector<std::string> g_debugRequests
{
    "input", "reload"
};
enum DebugRequest : size_t
{
    DR_Input = 0U,
    DR_Reload
};

const std::vector<std::string> g_inputHands
{
    "left", "right"
};
enum InputHands : size_t
{
    IH_LeftHand = 0U,
    IH_RightHand
};

const std::vector<std::string> g_buttonTypes
{
    "button", "axis"
};
enum ButtonTypes : size_t
{
    BT_Button = 0U,
    BT_Axis
};

const std::vector<std::string> g_buttonNames
{
    "a", "b", "system"
};
enum ButtonNames : size_t
{
    BN_A = 0U,
    BN_B,
    BN_System
};

const std::vector<std::string> g_axisNames
{
    "thumbstick", "touchpad"
};
enum AxisNames : size_t
{
    AN_Thumbstick = 0U,
    AN_Touchpad
};

const std::vector<std::string> g_buttonStates
{
    "none", "touched", "clicked"
};
enum ButtonStates : size_t
{
    BS_None = 0U,
    BS_Touched,
    BS_Clicked
};

const char* const CServerDriver::ms_interfaces[]
{
    vr::ITrackedDeviceServerDriver_Version,
        vr::IServerTrackedDeviceProvider_Version,
        nullptr
};

CServerDriver::CServerDriver()
{
    m_leapPoller = nullptr;
    m_connectionState = false;
    for(size_t i = 0U; i < LCH_Count; i++) m_controllers[i] = nullptr;
    m_leapStation = nullptr;
}

CServerDriver::~CServerDriver()
{
}

// vr::IServerTrackedDeviceProvider
vr::EVRInitError CServerDriver::Init(vr::IVRDriverContext *pDriverContext)
{
    VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext)
        CDriverConfig::Load();

    // Relay device for events from leap_control
    m_leapStation = new CLeapStation(this);
    vr::VRServerDriverHost()->TrackedDeviceAdded(m_leapStation->GetSerialNumber().c_str(), vr::TrackedDeviceClass_TrackingReference, m_leapStation);

    m_controllers[LCH_Left] = new CLeapControllerIndex(CLeapController::CH_Left);
    m_controllers[LCH_Right] = new CLeapControllerIndex(CLeapController::CH_Right);

    for(size_t i = 0U; i < LCH_Count; i++)
    {
        vr::VRServerDriverHost()->TrackedDeviceAdded(m_controllers[i]->GetSerialNumber().c_str(), vr::TrackedDeviceClass_Controller, m_controllers[i]);
    }

    m_leapPoller = new CLeapPoller();
    if(m_leapPoller->Initialize())
    {
        m_leapPoller->SetTrackingMode(_eLeapTrackingMode::eLeapTrackingMode_HMD);
        m_leapPoller->SetPolicy(eLeapPolicyFlag::eLeapPolicyFlag_OptimizeHMD, eLeapPolicyFlag::eLeapPolicyFlag_OptimizeScreenTop);
    }

    // Start leap_control
    std::string l_path(g_modulePath);
    l_path.erase(l_path.begin() + l_path.rfind('\\'), l_path.end());

    std::string l_appPath(l_path);
    l_appPath.append("\\leap_control.exe");

    STARTUPINFOA l_infoProcess = { 0 };
    PROCESS_INFORMATION l_monitorInfo = { 0 };
    l_infoProcess.cb = sizeof(STARTUPINFOA);
    CreateProcessA(l_appPath.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, l_path.c_str(), &l_infoProcess, &l_monitorInfo);

    hardware_update_thread_ = std::thread(&CServerDriver::HardwareUpdateThread, this);
    hardware_update_thread_.detach();
    /*
    if (!curl) curl = curl_easy_init();
    if (!curl) {
        DriverLog("Failed to initialize libcurl\n");
    }
    else {
        DriverLog("libcurl initialized\n");
        hardware_update_thread_ = std::thread(&CServerDriver::HardwareUpdateThread, this);
        hardware_update_thread_.detach();
        //hardware_update_thread_ = std::thread(&CLeapControllerIndex::HardwareUpdateThread, this);
        //hardware_update_thread_.detach();
        
        CURLcode res;
        // Set the URL for the POST request
        try {
            const char* url = "http://localhost:5147/api/v1/port";
            curl_easy_setopt(curl, CURLOPT_URL, url);

            // Set the callback function to handle the response
            std::string response;
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CServerDriver::WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            // Perform the HTTP POST request
            res = curl_easy_perform(curl);

            // Check for errors
            if (res != CURLE_OK) {
                printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }
            else {
                // Print the response
                //printf("%s\n", response);
                DriverLog("Parsing Controller Data");
                CServerDriver::parseString(response);
                //cout << response << endl;
            }

        }
        catch (...) {
            printf("ERROR on Read");
        }
    }
    */

    return vr::VRInitError_None;
}

void CServerDriver::Cleanup()
{
    for(size_t i = 0U; i < LCH_Count; i++)
    {
        delete m_controllers[i];
        m_controllers[i] = nullptr;
    }
    delete m_leapStation;
    m_leapStation = nullptr;

    m_leapPoller->Terminate();
    delete m_leapPoller;
    m_leapPoller = nullptr;

    VR_CLEANUP_SERVER_DRIVER_CONTEXT()
}

const char* const* CServerDriver::GetInterfaceVersions()
{
    return ms_interfaces;
}

void CServerDriver::RunFrame()
{
    CLeapController::UpdateHMDCoordinates();
    m_leapPoller->Update();

    if(m_connectionState != m_leapPoller->IsConnected())
    {
        m_connectionState = m_leapPoller->IsConnected();
        m_leapStation->SetTrackingState(m_connectionState ? CLeapStation::TS_Connected : CLeapStation::TS_Search);
        for(size_t i = 0U; i < LCH_Count; i++)
        {
            m_controllers[i]->SetEnabled(m_connectionState);
        }

        m_leapPoller->SetTrackingMode(_eLeapTrackingMode::eLeapTrackingMode_HMD);
        m_leapPoller->SetPolicy(eLeapPolicyFlag::eLeapPolicyFlag_OptimizeHMD, eLeapPolicyFlag::eLeapPolicyFlag_OptimizeScreenTop);
    }

    LEAP_HAND *l_hands[LCH_Count] = { nullptr };
    if(m_connectionState)
    {
        const LEAP_TRACKING_EVENT *l_frame = m_leapPoller->GetFrame();
        if(l_frame)
        {
            for(size_t i = 0U; i < l_frame->nHands; i++)
            {
                if(!l_hands[l_frame->pHands[i].type]) l_hands[l_frame->pHands[i].type] = &l_frame->pHands[i];
            }
        }
    }

    /*
    DriverLog("TEST 1");
    if (!curl) curl = curl_easy_init();
    CURLcode res;
    // Set the URL for the POST request
    try {
        const char* url = "http://localhost:5147/api/v1/port";
        curl_easy_setopt(curl, CURLOPT_URL, url);
        DriverLog("TEST 2");

        // Set the callback function to handle the response
        std::string response;
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CServerDriver::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        DriverLog("TEST 3");

        // Perform the HTTP POST request
        if (curl) {
            res = curl_easy_perform(curl);

            DriverLog("TEST 4");

            // Check for errors
            if (res != CURLE_OK) {
                printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }
            else {
                // Print the response
                //printf("%s\n", response);
                DriverLog("Parsing Controller Data");
                CServerDriver::parseString(response);
                //cout << response << endl;
            }
        }

    }
    catch (...) {
        DriverLog("ERROR on Read");
    }
    */

    // Update devices
    for(size_t i = 0U; i < LCH_Count; i++)
    {
        m_controllers[i]->RunFrame(l_hands[i]);
    }
    m_leapStation->RunFrame();
}

bool CServerDriver::ShouldBlockStandbyMode()
{
    return false;
}

void CServerDriver::EnterStandby()
{
}

void CServerDriver::LeaveStandby()
{
}

// CServerDriver
void CServerDriver::ProcessExternalMessage(const char *p_message)
{
    std::stringstream l_stream(p_message);
    std::string l_event;

    // Scary stuff
    l_stream >> l_event;
    if(!l_stream.fail() && !l_event.empty())
    {
        switch(ReadEnumVector(l_event, g_debugRequests))
        {
            case DR_Input:
            {
                std::string l_inputHand;
                l_stream >> l_inputHand;
                if(!l_stream.fail() && !l_inputHand.empty())
                {
                    size_t l_inputHandIndex = ReadEnumVector(l_inputHand, g_inputHands);
                    if(l_inputHandIndex != std::numeric_limits<size_t>::max())
                    {
                        std::string l_buttonType;
                        l_stream >> l_buttonType;
                        if(!l_stream.fail() && !l_buttonType.empty())
                        {
                            size_t l_buttonTypeIndex = ReadEnumVector(l_buttonType, g_buttonTypes);
                            if(l_buttonTypeIndex != std::numeric_limits<size_t>::max())
                            {
                                switch(l_buttonTypeIndex)
                                {
                                    case ButtonTypes::BT_Button:
                                    {
                                        std::string l_buttonName;
                                        l_stream >> l_buttonName;
                                        if(!l_stream.fail() && !l_buttonName.empty())
                                        {
                                            size_t l_buttonNameIndex = ReadEnumVector(l_buttonName, g_buttonNames);
                                            if(l_buttonNameIndex != std::numeric_limits<size_t>::max())
                                            {
                                                std::string l_buttonState;
                                                l_stream >> l_buttonState;
                                                if(!l_stream.fail() && !l_buttonState.empty())
                                                {
                                                    size_t l_buttonStateIndex = ReadEnumVector(l_buttonState, g_buttonStates);
                                                    if(l_buttonStateIndex != std::numeric_limits<size_t>::max())
                                                    {
                                                        switch(l_buttonNameIndex)
                                                        {
                                                            case ButtonNames::BN_A:
                                                            {
                                                                m_controllers[l_inputHandIndex]->SetButtonState(CLeapControllerIndex::IB_ATouch, l_buttonStateIndex >= ButtonStates::BS_Touched);
                                                                m_controllers[l_inputHandIndex]->SetButtonState(CLeapControllerIndex::IB_AClick, l_buttonStateIndex >= ButtonStates::BS_Clicked);
                                                            } break;
                                                            case ButtonNames::BN_B:
                                                            {
                                                                m_controllers[l_inputHandIndex]->SetButtonState(CLeapControllerIndex::IB_BTouch, l_buttonStateIndex >= ButtonStates::BS_Touched);
                                                                m_controllers[l_inputHandIndex]->SetButtonState(CLeapControllerIndex::IB_BClick, l_buttonStateIndex >= ButtonStates::BS_Clicked);
                                                            } break;
                                                            case ButtonNames::BN_System:
                                                            {
                                                                m_controllers[l_inputHandIndex]->SetButtonState(CLeapControllerIndex::IB_SystemTouch, l_buttonStateIndex >= ButtonStates::BS_Touched);
                                                                m_controllers[l_inputHandIndex]->SetButtonState(CLeapControllerIndex::IB_SystemClick, l_buttonStateIndex >= ButtonStates::BS_Clicked);
                                                            } break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } break;

                                    case ButtonTypes::BT_Axis:
                                    {
                                        std::string l_axisName;
                                        l_stream >> l_axisName;
                                        if(!l_stream.fail() && !l_axisName.empty())
                                        {
                                            size_t l_axisNameIndex = ReadEnumVector(l_axisName, g_axisNames);
                                            if(l_axisNameIndex != std::numeric_limits<size_t>::max())
                                            {
                                                std::string l_buttonState;
                                                l_stream >> l_buttonState;
                                                if(!l_stream.fail() && !l_buttonState.empty())
                                                {
                                                    size_t l_buttonStateIndex = ReadEnumVector(l_buttonState, g_buttonStates);
                                                    if(l_buttonStateIndex != std::numeric_limits<size_t>::max())
                                                    {
                                                        glm::vec2 l_axisValues(0.f);
                                                        l_stream >> l_axisValues.x >> l_axisValues.y;
                                                        if(!l_stream.fail())
                                                        {
                                                            switch(l_axisNameIndex)
                                                            {
                                                                case AxisNames::AN_Thumbstick:
                                                                {
                                                                    m_controllers[l_inputHandIndex]->SetButtonState(CLeapControllerIndex::IB_ThumbstickTouch, l_buttonStateIndex >= ButtonStates::BS_Touched);
                                                                    m_controllers[l_inputHandIndex]->SetButtonState(CLeapControllerIndex::IB_ThumbstickClick, l_buttonStateIndex >= ButtonStates::BS_Clicked);
                                                                    m_controllers[l_inputHandIndex]->SetButtonValue(CLeapControllerIndex::IB_ThumbstickX, (l_buttonStateIndex >= ButtonStates::BS_Touched) ? l_axisValues.x : 0.f);
                                                                    m_controllers[l_inputHandIndex]->SetButtonValue(CLeapControllerIndex::IB_ThumbstickY, (l_buttonStateIndex >= ButtonStates::BS_Touched) ? l_axisValues.y : 0.f);
                                                                } break;
                                                                case AxisNames::AN_Touchpad:
                                                                {
                                                                    m_controllers[l_inputHandIndex]->SetButtonState(CLeapControllerIndex::IB_TrackpadTouch, l_buttonStateIndex >= ButtonStates::BS_Touched);
                                                                    m_controllers[l_inputHandIndex]->SetButtonValue(CLeapControllerIndex::IB_TrackpadForce, (l_buttonStateIndex == ButtonStates::BS_Clicked) ? 1.f : 0.f);
                                                                    m_controllers[l_inputHandIndex]->SetButtonValue(CLeapControllerIndex::IB_TrackpadX, (l_buttonStateIndex >= ButtonStates::BS_Touched) ? l_axisValues.x : 0.f);
                                                                    m_controllers[l_inputHandIndex]->SetButtonValue(CLeapControllerIndex::IB_TrackpadY, (l_buttonStateIndex >= ButtonStates::BS_Touched) ? l_axisValues.y : 0.f);
                                                                } break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } break;
                                }
                            }
                        }
                    }
                }
            } break;

            case DR_Reload:
                CDriverConfig::Load();
                break;
        }
    }
}

void CServerDriver::HardwareUpdateThread()
{
    while (is_active_)
    {
        CURL* curl = curl_easy_init();
        CURLcode res;
        // Set the URL for the POST request
        try {
            const char* url = "http://localhost:5147/api/v1/port";
            res = curl_easy_setopt(curl, CURLOPT_URL, url);

            // Set the callback function to handle the response
            std::string response;
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CServerDriver::WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            /*
            // Set the Accept header to specify the desired content type
            const char* acceptHeader = "Accept: text/plain"; // Specify the content type you expect
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, acceptHeader);
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            */
            
            // Perform the HTTP GET request
            res = curl_easy_perform(curl);

            // Check for errors
            if (res != CURLE_OK) {
                printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }
            else {
                // Print the response
                //printf("%s\n", response);
                CServerDriver::parseString(response);
                //cout << response << endl;
            }
            

        }
        catch (...) {
            DriverLog("ERROR on Read");
        }
        curl_easy_cleanup(curl);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}

bool lastGripState = false; 

void CServerDriver::parseString(std::string input) {
    //DriverLog("%s", input);
    std::istringstream outerStream(input);

    std::string control_cmd;
    char cmd_delim = '\t';

    while (std::getline(outerStream, control_cmd, cmd_delim)) {

        std::istringstream innerStream(control_cmd);
        std::string control_name;
        char element_delim = ';';

        if (!std::getline(innerStream, control_name, element_delim)) break;

        std::string control_data;

        if (!std::getline(innerStream, control_data, element_delim)) break;

        std::istringstream dataStream(control_data);
        std::string data_element;
        char data_delim = ':';
        int xVal, yVal, buttonVal;

        switch (control_name.at(0)) {
        case 'J':
            if (!std::getline(dataStream, data_element, data_delim)) break;
            xVal = std::stoi(data_element);
            m_controllers[0]->SetButtonState(CLeapControllerIndex::IB_TrackpadX, xVal);
            //m_buttons[IB_TrackpadX]->SetValue(xVal);
            //x.store(xVal);
            if (!std::getline(dataStream, data_element, data_delim)) break;
            yVal = std::stoi(data_element);
            m_controllers[0]->SetButtonState(CLeapControllerIndex::IB_TrackpadY, yVal);
            //m_buttons[IB_TrackpadY]->SetValue(yVal);
            //y.store(yVal);
            break;
        case 'B':
            buttonVal = std::stoi(control_data);

            switch (control_name[2]) {
            case 'B':
                //m_controllers[0]->SetButtonState(CLeapControllerIndex::IB_AClick, buttonVal);
                if (lastGripState != buttonVal) {
                    m_controllers[0]->SetGripState(buttonVal);
                    lastGripState = buttonVal;
                }
                //m_buttons[IB_AClick]->SetValue(buttonVal);
                //B_val.store(buttonVal);
                break;
            case 'G':
                m_controllers[0]->SetButtonState(CLeapControllerIndex::IB_BClick, buttonVal);
                //m_buttons[IB_BClick]->SetValue(buttonVal);
                //G_val.store(buttonVal);
                break;
            case 'R':
                m_controllers[0]->SetButtonState(CLeapControllerIndex::IB_SystemClick, buttonVal);
                //m_buttons[IB_SystemClick]->SetValue(buttonVal);
                //R_val.store(buttonVal);
                break;
            default:
                break;
            }
            break;
        case 'G':
            if (!std::getline(dataStream, data_element, data_delim)) break;
            //roll.store(std::stof(data_element));
            if (!std::getline(dataStream, data_element, data_delim)) break;
            //pitch.store(std::stof(data_element));
            if (!std::getline(dataStream, data_element, data_delim)) break;
            //yaw.store(std::stof(data_element));
            break;
        default:
            break;
        }
    }
}

size_t CServerDriver::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    if (contents == NULL || nmemb<0) {
        return 0; // Return 0 to indicate an error
    }
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}