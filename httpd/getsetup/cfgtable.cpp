// configure table used by setup pages

#include <stdio.h>

struct cfg_table {
    const char * section ;
    const char * key ;
    const char * jfield ;
    int   valuetype ;       //0: string, 1: integer, 2: float, 3, "on" if trun, 4: on if false
};

cfg_table system_table[] = {
#ifdef MDVR_APP
    // dvr_server_name
    {"system", "hostname", "dvr_server_name", 0},
#endif

#ifdef PWII_APP
    {"system", "id1", "vehicleid", 0},
    {"system", "id2", "district", 0},
    {"system", "serial", "unit", 0},
    {"system", "pw_recordmethod", "pw_recordmethod", 3},
#endif

#ifdef TVS_APP
    {"system", "tvs_licenseplate", "tvs_licenseplate", 0},
    {"system", "tvs_medallion", "tvs_medallion", 0},
    {"system", "tvs_ivcs_serial", "tvs_ivcs_serial", 0},
#endif

    {"system", "timezone", "dvr_time_zone", 0},

    // custom timezone string
    {"timezones", "Custom", "custom_tz", 0},

    // timing 
    {"system", "shutdowndelay", "shutdown_delay", 0},
    {"system", "standbytime", "standbytime", 0},
    {"system", "uploadingtime", "uploadingtime", 0},
    {"system", "archivetime", "archivetime", 0},
    {"system", "tracemarktime", "tracemarktime", 1},

    // minimun_disk_space
    {"system", "mindiskspace_percent", "minimun_disk_space", 0},
    // Number of camera
    {"system", "totalcamera", "totalcamera", 1},

#ifdef APP_PWZ8
    // Number of body camera
    {"system", "totalbodycam", "totalbodycam", 1},
#endif

    // pre_lock_time
    {"system", "prelock", "pre_lock_time", 1},
    // post_lock_time
    {"system", "postlock", "post_lock_time", 1},

    // en_file_encryption
    {"system", "fileencrypt", "en_file_encryption", 3},

#ifdef APP_PWZ8
    // sd camera
    {"system", "camsd", "camsd", 3},
#endif

    // gpslog enable ( reversed "on")
    {"glog", "gpsdisable", "en_gpslog", 4},

    // gps port
    {"glog", "serialport", "gpsport", 0},

    // gps baud rate
    {"glog", "serialbaudrate", "gpsbaudrate", 0},

    // g-force data
    {"glog", "gforce_log_enable", "gforce_enable", 3},

    {"io", "gsensor_forward", "gforce_forward", 0},
    {"io", "gsensor_upward", "gforce_upward", 0},
    {"io", "gsensor_mountangle", "gforce_mountangle", 0},

    // testing, let web display angle calibrate option
    {"io", "gsensor_show_mountangle", "show_mountangle", 3},
    
    {"io", "gsensor_crashdata", "gforce_crashdata", 3},

    // gforce trigger value (peak value?)
    {"io", "gsensor_forward_trigger", "gforce_forward_trigger", 1},
    {"io", "gsensor_backward_trigger", "gforce_backward_trigger", 1},
    {"io", "gsensor_right_trigger", "gforce_right_trigger", 1},
    {"io", "gsensor_left_trigger", "gforce_left_trigger", 1},
    {"io", "gsensor_down_trigger", "gforce_downward_trigger", 1},
    {"io", "gsensor_up_trigger", "gforce_upward_trigger", 1},

    // base value (what?)
    {"io", "gsensor_forward_base", "gforce_forward_base", 1},
    {"io", "gsensor_backward_base", "gforce_backward_base", 1},
    {"io", "gsensor_right_base", "gforce_right_base", 1},
    {"io", "gsensor_left_base", "gforce_left_base", 1},
    {"io", "gsensor_down_base", "gforce_downward_base", 1},
    {"io", "gsensor_up_base", "gforce_upward_base", 1},

    // crash value
    {"io", "gsensor_forward_crash", "gforce_forward_crash", 1},
    {"io", "gsensor_backward_crash", "gforce_backward_crash", 1},
    {"io", "gsensor_right_crash", "gforce_right_crash", 1},
    {"io", "gsensor_left_crash", "gforce_left_crash", 1},
    {"io", "gsensor_down_crash", "gforce_downward_crash", 1},
    {"io", "gsensor_up_crash", "gforce_upward_crash", 1},

    // Video output selection
    {"VideoOut", "startchannel", "videoout", 0},

    {NULL, NULL, NULL, 0}
};


cfg_table camera_table[] = {
    // enable_camera
    {NULL, "enable", "enable_camera", 3},

    // camera_name
    {NULL, "name", "camera_name", 0},
    // m_enablejicaudio, record audio in JIC mode
    {NULL, "enablejicaudio", "enablejicaudio", 3},
    // camera_type
    {NULL, "type", "camera_type", 1},
    // ip camera_url
    {NULL, "stream_URL", "ipcamera_url", 0},
    // Physical channel
    {NULL, "channel", "channel", 1},
    // recording_mode
    {NULL, "recordmode", "recording_mode", 1},

#ifdef APP_PWZ5
    // force record channel (camera direction)
    {NULL, "forcerecordchannel", "forcerecordchannel", 1},
#endif
    // # resolution, 0:CIF, 1:2CIF, 2:DCIF, 3:4CIF
    // resolution
    {NULL, "resolution", "resolution", 1},
    // frame_rate
    {NULL, "framerate", "frame_rate", 1},

    // bit_rate
    {NULL, "bitrate", "bit_rate", 0},
    // # picture quality, 0:lowest, 10:highest
    // picture_quaity
    {NULL, "quality", "picture_quaity", 1},
    // picture controls
    {NULL, "brightness", "brightness", 1},
    {NULL, "contrast", "contrast", 1},
    {NULL, "saturation", "saturation", 1},
    {NULL, "hue", "hue", 1},

    // g-force trigger recording
    // gforce_trigger
    {NULL, "gforce_trigger", "gforce_trigger", 3},
    {NULL, "gforce_trigger_value", "gforce_trigger_value", 2},

    // speed_trigger
    {NULL, "speed_trigger", "speed_trigger", 3},
    {NULL, "speed_trigger_value", "speed_trigger_value", 2},

    // pre_recording_time
    {NULL, "prerecordtime", "pre_recording_time", 1},
    // post_recording_time
    {NULL, "postrecordtime", "post_recording_time", 1},

    // show_gps
    {NULL, "showgps", "show_gps", 3},

    // gpsunit
    // speed_display
    {NULL, "gpsunit", "speed_display", 0},
    {NULL, "showgpslocation", "show_gps_coordinate", 3},

#ifdef TVS_APP

    // Medallion on OSD
    {NULL, "show_medallion", "show_medallion", 3},

    // License plate on OSD
    {NULL, "show_licenseplate", "show_licenseplate", 3},

    // IVCS on OSD
    {NULL, "show_ivcs", "show_ivcs", 3},

    // IVCS on OSD
    {NULL, "show_cameraserial", "show_cameraserial", 3},

#endif

#ifdef PWII_APP
    {NULL, "show_vri", "show_vri", 3},
    {NULL, "show_policeid", "show_policeid", 3},
#endif

    {NULL, "show_gforce", "show_gforce", 3},

    {NULL, "recordalarmpattern", "record_alarm_mode", 0},
    {NULL, "recordalarm", "record_alarm_led", 0},

    // video_lost_alarm_mode
    {NULL, "videolostalarmpattern", "video_lost_alarm_mode", 0},
    {NULL, "videolostalarm", "video_lost_alarm_led", 0},

    // motion_alarm_mode
    {NULL, "motionalarmpattern", "motion_alarm_mode", 0},
    {NULL, "motionalarm", "motion_alarm_led", 0},

    // motion_sensitivity
    {NULL, "motionsensitivity", "motion_sensitivity", 0},

    // disableaudio
    {NULL, "disableaudio", "disableaudio", 3},

    // key interval
    {NULL, "key_interval", "key_interval", 0},

    {NULL, NULL, NULL, 0}
};


cfg_table sensor_table[] = {
};
