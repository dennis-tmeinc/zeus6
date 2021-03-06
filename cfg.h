/* generic definition file */

// option to enable ioprocess display mcu debugging message
// #define MCU_DEBUG  (1)

// to enable dvrsvr network message
// #define NETDBG

// to enable power cycling test firmware
// #define POWERCYCLETEST

// application PW
//#define MDVR_APP
#define PWII_APP
//#define TVS_APP

#define MCU_ZEUS

// yet another g-force
//#define SUPPORT_YAGF

#define	APPNAME	"PWZ5"
#define APP_PWZ5
#define APP_PWZ8

// #define SUPPORT_SCREEN   
#define SUPPORT_IPCAM

#define ZEUS	8
#define ZEUS8

// default config file
#define	CFG_DEFFILE	"/davinci/dvr/defconf"
#define	CFG_FILE	"/davinci/dvr/dvrsvr.conf"

// applications dir
#define APP_DIR		"/davinci/dvr"
// where dvr var files created
#define VAR_DIR		"/var/dvr"

// shared io map file
#define DVRIOMAP    "/dvriomap"

#define WWWROOT	"/var/www"
#define WWWSERIALFILE "/var/www/wwwserialnofile"

// bodycam support
#define SUPPORT_BODYCAM	(1)
#define BODYCAM_PORT (15214)

#define NETDBG
