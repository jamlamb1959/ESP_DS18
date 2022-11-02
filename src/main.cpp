#include <Arduino.h>

#define MAXRUNTIME  300
#define CONNECTEDRUNTIME    10

#ifdef ESP32
#ifdef LED_BUILTIN
#define ID "Heltec"
// #define USE_ESPNOW
#define USE_OTA
// #define USE_PUBSUB
#define USE_WIFI
#define USE_DS18
#define USE_MESH
#define SLEEPTIME   300
#else

#define ID "ESP32"
// #define USE_ESPNOW
#define USE_OTA
// #define USE_PUBSUB
#define USE_WIFI
#define USE_DS18
#define SLEEPTIME   300
#define USE_MESH
#endif
#else

#define ID "ESP8266"
// #define USE_ESPNOW
// #define USE_OTA
#define USE_PUBSUB
#define USE_WIFI
// #define USE_DS18
#define SLEEPTIME   300
#endif



#ifdef ESP32
#include <WiFi.h>
#ifdef USE_OTA
#include <ESP32httpUpdate.h>
#endif
#else
#include <ESP8266WiFi.h>
// #include <ESP8266WiFiMulti.h>
#ifdef USE_OTA
#include <ESP8266httpUpdate.h>
#endif
#endif

#ifdef USE_MESH
#include <painlessMesh.h>

#define MESH_SSID "pharmdata"
#define MESH_PASSWORD "4026892842"
#define MESH_PORT 5555

std::list< uint32_t > gwAddr_g;
#endif

#ifdef ESP32
#ifndef LED_BUILTIN
/*
** Non-Heltec
*/
#define LED_BUILTIN     2
#define D1 17
#define D2 21
#define PROG        "firmware/esp32dev/ESP_DS18/firmware"
#else
/*
** Heltec
*/
#define PROG        "firmware/heltec_wifi_lora_32_V2/ESP_DS18/firmware"
#define D1 13
#define D2 12
#endif

#endif

#define ARDUINOJSON_ENABLE_STD_STRING 1

#ifdef USE_WIFI
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#endif

#ifdef USE_PUBSUB
#include <PubSubClient.h>
#endif

#ifdef USE_DS18
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

#ifdef USE_ESPNOW
#ifndef USE_WIFI
#include <ESP8266WiFi.h>
#endif
#define WIFI_MODE_STA WIFI_STA

#ifdef USE_ESPNOW
#include <QuickEspNow.h>
#endif

static uint8_t broadcast_g[ 6 ] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
#endif

#define BLINKINV    300
#define MESHPREFIX "pharmdata"
#define MESHPASSWD "4026892842"
#define MESHPORT    1977
#define MQTTHOST    "pharmdata.ddns.net"
#define MQTTPORT    1883
#ifdef ESP32
#define PROGNAME    "ESP_DS18"
#else
#define PROG        "d1_mini/ESP_DS18"
#define PROGNAME    "ESP8266_DS18"
#endif
#define RESTARTINV  3600
#define RPTINV      3
#define RUNTIME     30

#define SENSORPIN   D1 /* D1 */
#define SENSORPWR   D2 /* D2 */
#define WIFICONNECTTMO  10000

static unsigned long sleepTime_g = 0;
static char mac_g[ 40 ];
#ifdef USE_WIFI
static char logTopic_g[ 40 ];
static char ssid_g[ 64 ];
#endif
#ifdef USE_MESH

Scheduler userScheduler;

typedef std::function<void(String &from, String &msg)> namedReceivedCallback_t;

class MeshWrapper 
        : public painlessMesh
    {
  public:
    MeshWrapper(
            ) 
        {
        auto cb = [this](
                uint32_t aFromAddr, String & aMsg
                ) 
            {
            if ( aMsg == "IM" )
                {
                bool found = false;

                Serial.printf( "%u(0x%X), Process IM\n", aFromAddr, aFromAddr );

                for( uint32_t gw : gwAddr_g )
                    {
                    if ( gw == aFromAddr )
                        {
                        found = true;
                        }
                    }
    
                if ( !found )
                    {
                    gwAddr_g.push_back( aFromAddr );

                    if ( gwAddr_g.size() == 1 )
                        {
                        sleepTime_g = millis() + (CONNECTEDRUNTIME * 1000);
                        }
                    }

                Serial.printf( "gwAddr_g: " );
                for( uint32_t gw : gwAddr_g )
                    {
                    Serial.printf( "%u(0x%X)", gw, gw );
                    }
                Serial.println( "" );

                return;
                }
            
            Serial.printf( "(0x%X) - %s\n",
                    aFromAddr, aMsg.c_str() );
            };

        painlessMesh::onReceive( cb );
        }

    String getName(
            ) 
        {
        return nodeName;
        }

    void setName(
            String &name
            ) 
        {
        nodeName = name;
        }

    using painlessMesh::sendSingle;

    bool sendSingle(
            String &aName, 
            String &aMsg
            ) 
        {
        // Look up name
        for (auto && pr : nameMap) 
            {
            if (aName.equals(pr.second)) 
                {
                uint32_t to = pr.first;
                return painlessMesh::sendSingle(to, aMsg);
                }
            }
        return false;
        }

    virtual void stop() 
        {
        painlessMesh::stop();
        }

    virtual void onReceive(
            painlessmesh::receivedCallback_t onReceive
            ) 
        {
        userReceivedCallback = onReceive;
        }

    void onReceive(
            namedReceivedCallback_t onReceive
            ) 
        {
        userNamedReceivedCallback = onReceive;
        }
    
  protected:
    String nodeName;
    std::map<uint32_t, String> nameMap;

    painlessmesh::receivedCallback_t userReceivedCallback;
    namedReceivedCallback_t          userNamedReceivedCallback;
    };

static MeshWrapper mesh;
static String nodeName_g;

static void _setupMesh(
        )
    {
    Serial.printf( "_setupMesh(enterd)\n" );
#ifdef ESP32
    WiFi.disconnect( false, true );
#else
    WiFi.disconnect( false );
#endif

    mesh.setDebugMsgTypes( COMMUNICATION | CONNECTION | ERROR | MESH_STATUS );

    mesh.init( MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_STA, 6 );

    mesh.setName( nodeName_g );

    mesh.onReceive( []( 
            uint32_t aFrom, 
            String & aMsg
            )
        {
        Serial.printf( "onReceive - aFrom: 0x%X, %s\n",
                aFrom, aMsg.c_str() );
        } );

    mesh.onReceive( []( 
            String & aFrom, 
            String & aMsg
            )
        {
        Serial.printf( "onReceive - aFrom: %s, %s\n",
                aFrom.c_str(), aMsg.c_str() );
        } );

    mesh.onChangedConnections( []() 
        {
        Serial.printf( "\n(onChangedCOnnections)\n%s\n",
                mesh.subConnectionJson( true ).c_str() );
        } );
    }

#endif

static unsigned long nxtRpt_g = 0;

#ifdef USE_DS18
static OneWire ds( SENSORPIN );
static DallasTemperature dt_g( &ds );
#endif


typedef struct
{
  const char * ssid;
  const char * passwd;
  const char * updateAddr;
} WiFiInfo;

WiFiInfo wifiInfo_g[] =
{
  { "s1616", "4026892842", "pharmdata.ddns.net" },
  { "lambhome", "4022890568", "pharmdata.ddns.net" },
  { "sheepshed-mifi", "4026892842", "pharmdata.ddns.net" },
  { "Jimmy-MiFi", "4026892842", "pharmdata.ddns.net" },
  { NULL }
};

static WiFiInfo * wifi_g = NULL;

#ifdef USE_OTA
#ifdef ESP32

static void _progress(
        size_t aDone,
        size_t aTotal
        )
    {
    static bool ledState = false;
    static unsigned int cntr = 0;

    cntr ++;
    if ( (cntr % 5) == 0 )
        {
        digitalWrite( LED_BUILTIN, ledState ? HIGH : LOW );
        ledState = !ledState;

        size_t pct10;

        pct10 = (aDone * 1000) /aTotal;

        Serial.printf( "%u/%u(%%%d.%d)\r", aDone, aTotal, pct10 / 10, pct10 % 10 );
        }
    }

void _checkUpdate(
        )
    {
    Serial.printf( "(%d) checkUpdate - (repo: %s) file: %s\r\n", 
            __LINE__, (wifi_g != NULL) ? wifi_g->updateAddr : "NULL", PROG );

    if ( wifi_g != NULL )
        {
        Update.onProgress( _progress );

        ESPhttpUpdate.rebootOnUpdate( true );

        t_httpUpdate_return ret = ESPhttpUpdate.update( wifi_g->updateAddr, 80,
                "/firmware/update.php", PROG );

        if ( ESPhttpUpdate.getLastError() != 0 )
            {
	        Serial.printf( "(%d) %s\r\n", 
                    ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str() );
            }

        switch( ret )
            {
            case HTTP_UPDATE_FAILED:
                // Serial.printf( "HTTP_UPDATE_FAILED(%d)\r\n", ret );
                break;

            case HTTP_UPDATE_NO_UPDATES:
                // Serial.printf( "HTTP_UPDATE_NO_UPDATES(%d)\r\n", ret );
                break;

            case HTTP_UPDATE_OK:
                // Serial.printf( "HTTP_UPDATE_OK(%d)\r\n", ret );
                break;
            }
        }
    }
#else
static void _checkUpdate();

static void _updateStart(
        )
    {
    Serial.print( "\r\n\nStart update\r\n" );
    digitalWrite( LED_BUILTIN, HIGH );
    }

static void _updateEnd(
        )
    {
    Serial.print( "\r\nFinished\r\n\n" );
    digitalWrite( LED_BUILTIN, HIGH );
    }

static void _updateProgress(
        int aCur,
        int aTot
        )
    {
    static bool blink = false;
    static unsigned int cnt = 0;

    cnt ++;
    digitalWrite( LED_BUILTIN, (blink) ? HIGH : LOW );
    blink = !blink;

    Serial.printf( "\r%d/%d(%d)", 
            aCur, aTot, (aTot != 0) ? (aCur * 100) / aTot : 0 );
    }

static void _updateError(
        int anErr
        )
    {
    Serial.printf( "[cb] _updateError Error: %d\n", anErr );
    }
#endif
#endif

#ifdef USE_PUBSUB
static WiFiClient psClient_g;

static void _cb( char * aTopic, byte * aPayload, unsigned int aPayloadLen );

static PubSubClient ps_g( MQTTHOST, MQTTPORT, _cb, psClient_g );
#endif

static void _log(
        const char * const aFrmt,
        ...
        )
    {
    static char buf[ 400 ];

    va_list ap;

    va_start( ap, aFrmt );
    vsnprintf( buf, sizeof( buf ), aFrmt, ap );
    va_end( ap );
      
    Serial.println( buf );

#ifdef USE_PUBSUB
#ifdef USE_WIFI
    if ( ps_g.connected() )
        {
        ps_g.publish( logTopic_g, buf );
        }
#endif
#endif
    }

#ifdef USE_PUBSUB
static void _waitMQTT(
        const unsigned long aSeconds
        )
    {
#ifdef USE_WIFI
    unsigned long et = millis() + (aSeconds * 1000);

    while( millis() < et )
        {
        if ( ps_g.connected() )
            {
            ps_g.loop();
            }
        delay( 100 );
        }
#endif
    }
#endif
            
static void _getTkn(
        String & aTkn,
        const char * & aPtr,
        const char * const aSepList
        )
    {
    aTkn = "";

    for( ; *aPtr != '\0'; aPtr ++ )
        {
        if ( strchr( aSepList, *aPtr ) != NULL )
            {
            aPtr ++;
            break;
            }

        aTkn += *aPtr;
        }
    }


static void _prcsCmd(
        const char * aCmd
        )
    {
    if ( aCmd[ 0 ] != '-' )
        {
        return;
        }

    switch( aCmd[ 1 ] )
        {
        case 'B':
            _prcsCmd( aCmd + 2 );
            break;

        case 'C':
            {
            const char * wp = aCmd + 2;

            String subCmd;

            _getTkn( subCmd, wp, " \r\n\t" );

            if ( subCmd == "restart" )
                {
                String prog;

                _getTkn( prog, wp, " \r\n\t" );
                if ( (prog == PROGNAME) || (prog == mac_g) )
                    {
                    delay( 3000 );
                    ESP.restart();
                    }

                if ( prog == "CHECK" )
                    {
#ifdef USE_PUBSUB
                    _waitMQTT( 1 );
#endif
#ifdef USE_OTA
                    _checkUpdate();
#endif
                    }

                }
            }
            break;
        }
    }
            
        
#ifdef USE_PUBSUB
#ifdef USE_WIFI
static void _cb(
        char * aTopic,
        byte * aPayload,
        unsigned int aPayloadLen
        )
    {
    const char * wp;

    String payload;
    String topic;

    unsigned int idx;
    
    topic = aTopic;
    for( idx = 0; idx < aPayloadLen; idx ++ )
        {
        payload += (char) aPayload[ idx ];
        }

    Serial.printf( "topic: %s\npayload: %s\n", topic.c_str(), payload.c_str() );

    wp = payload.c_str();

    _prcsCmd( wp );
    }
#endif
#endif

#ifdef USE_PUBSUB
#ifdef USE_WIFI
static void _reconnect(
        )
    {
    static int attempt = 0;
    int retries;

    String ssid;

    String clientId;

    clientId = "MQTT-";
    clientId += mac_g;
    clientId += '-';
    clientId += String( attempt ++ );

    if ( ps_g.connected() || (wifi_g == NULL) )
        {
        return;
        }

    Serial.printf( "%s, Attempt connect.\n", clientId.c_str() );
    delay( 1000 );

    for( retries = 0; (retries < 10) && (!ps_g.connected()); retries ++ )
        {
        if ( ps_g.connect( clientId.c_str() ) )
            {
            Serial.println( "\rConnected.\r" );

            _log( "\n" PROGNAME ":\n"
                    "TIMESTAMP: " __TIMESTAMP__ "\n"
                    "mac: %s\n"
                    "ssid: %s\n"
                    , mac_g, ssid_g );

            ps_g.subscribe( "/MANAGE", 0 );
            }
        else
            {
            Serial.print( "failed, rc: " ); Serial.println( ps_g.state() );
            delay( 2000 );
            }
        }

    if ( !ps_g.connected() )
        {
        ESP.restart();
        }
    }
#endif
#endif

#ifdef USE_OTA
#ifdef ESP8266
static void _checkUpdate(
        )
    {
    int er;
    String msg;

    WiFiClient client;

    ESP8266HTTPUpdate hu( 30000 );

    if ( wifi_g == NULL )
        {
        Serial.printf( "(_checkUpdate) - wifi_g not set.\n" );
        return;
        }

    Serial.printf( "_checkUpdate - wifi_g: %s, PROG: %s\r\n", wifi_g->updateAddr, PROG );

    hu.onStart( _updateStart );
    hu.onEnd( _updateEnd );
    hu.onProgress( _updateProgress );
    hu.onError( _updateError );

    t_httpUpdate_return ret = hu.update( 
            client, wifi_g->updateAddr, 80, "/firmware/update.php", PROG );

    switch( ret )
        {
        case HTTP_UPDATE_FAILED:
            er = ESPhttpUpdate.getLastError();
            msg = ESPhttpUpdate.getLastErrorString();

            Serial.println( "[update] failed." );
            Serial.printf( "   Error(%d): %s\n",
                    ESPhttpUpdate.getLastError(),
                    ESPhttpUpdate.getLastErrorString().c_str() );

            _log( "OTA failed. err(%d): %s", er, msg.c_str() );
#ifdef USE_PUBSUB
            _waitMQTT( 5 );
#endif

            ESP.restart();
            break;

        case HTTP_UPDATE_NO_UPDATES:
            _log( "[update] No Updates." );
            break;

        case HTTP_UPDATE_OK:
            _log( "[update] Ok" );
            break;

        default:
            _log( "[update] (Unexpected) ret: %d", ret );
            break;
        }

    delay( 2000 );
    }
#endif
#endif

#ifdef USE_ESPNOW
static void _setupESPNOW(
        )
    {
    WiFi.mode( WIFI_MODE_STA );

    WiFi.disconnect( false );
    wifi_g = NULL;

    Serial.printf( "_setupESPNOW - SSID: %s, channel: %d\r\n", 
            WiFi.SSID().c_str(), WiFi.channel() );
    Serial.printf( "IP address: %s\r\n", WiFi.localIP().toString().c_str() );
    Serial.printf( "MAC address: %s\r\n", WiFi.macAddress().c_str() );

    Serial.printf( "Explicitly set ESPNOW channel to 11\r\n" );

    quickEspNow.setChannel( 11 );

    quickEspNow.begin( 11, WIFI_IF_STA );
    }

#endif

static unsigned long stTime_g = 0;

void setup(
        ) 
    {
    int cntDown;

    /*
    ** The first instruction.
    */
    stTime_g = millis();

    Serial.begin( 115200 );
    delay( 1000 );

    // Serial.setDebugOutput( true );

    for( cntDown = 5; cntDown > 0; cntDown -- )
        {
        Serial.printf( "(%d) \r", cntDown );
        delay( 1000 );
        }
    Serial.println( "" ); 


#ifdef USE_WIFI
    int cnt;
    int routerCount = 0;

    /*
    ** First attempt to connect to one of the configured wifi networks.
    */
    WiFiInfo * wi;

    // WiFi.mode( WIFI_STA );

    Serial.printf( "%s, Attempt Connect WiFi\r\n", mac_g );
    delay( 100 );

    for( wi = wifiInfo_g; wi->ssid != NULL; wi ++ )
        {
        routerCount ++;
        Serial.printf( "ssid: %s\r\n", wi->ssid );

        WiFi.begin( wi->ssid, wi->passwd );
        for( cnt = 0; (cnt < 100) && (WiFi.status() != WL_CONNECTED); cnt ++ )
            {
            delay( 100 );
            Serial.print( '.' );
            }
        Serial.printf( "\r\nDone(RouterCount: %d)(%d)\r\n", routerCount, cnt );

        if ( WiFi.status() == WL_CONNECTED )
            {
            break;
            }
        }

    strcpy( mac_g, WiFi.macAddress().c_str() );

    strcpy( logTopic_g, "/LOGESP/" );
    strcat( logTopic_g, mac_g );

    if ( WiFi.status() == WL_CONNECTED )
        {
        strcpy( ssid_g, WiFi.SSID().c_str() );

        wifi_g = wi;

        Serial.printf( "ssid: %s   Connected\r\n", ssid_g );

#ifdef USE_PUBSUB
        _reconnect();
#endif
#ifdef USE_OTA
        _checkUpdate();
#endif
        }
    else
        {
        Serial.printf( "WIFI not connected.\n" );
        }
#endif

    pinMode( LED_BUILTIN, OUTPUT );

#ifdef USE_DS18
    pinMode( SENSORPWR, OUTPUT );
    digitalWrite( SENSORPWR, HIGH );
    pinMode( SENSORPIN, INPUT_PULLUP );

    dt_g.begin();
#endif

#ifdef USE_ESPNOW
    _setupESPNOW();

    char buf[ 200 ];

    sprintf( buf, "{\"MAC\":\"%s\",\"boot\":\"(%s) %s %s\",\"PROGNAME\":\"%s\",\"ID\":\"%s\"}",
            mac_g, __FILE__, __DATE__, __TIME__, PROGNAME, ID );
    quickEspNow.send( broadcast_g, (uint8_t *) buf, strlen( buf ) );
    Serial.println( buf );
#endif

#ifdef USE_MESH
    _setupMesh();
#endif

    sleepTime_g = millis() + (MAXRUNTIME * 1000);

    _log( "Setup Complete" );
    }

static bool blink_g = false;
static unsigned long nxtBlink_g = 0;

#ifdef ESP32
RTC_DATA_ATTR unsigned int rptCnt_g;
#else
static unsigned int rptCnt_g = 0;
#endif

void loop(
        ) 
    {
    char buf[ 300 ];

#ifdef USE_MESH
    mesh.update();
#endif

#ifdef ESP8266
    ESP.wdtFeed();
#endif

#ifdef USE_PUBSUB
#ifdef USE_WIFI
    /*
    ** if WIFI is connected.
    */
    if ( wifi_g != NULL )
        {
        if ( ps_g.connected() )
            {
            ps_g.loop();
            }
        else
            {
            _reconnect();
            ps_g.loop();
            }
        }
#endif
#endif

    if ( millis() > nxtBlink_g )
        {
        nxtBlink_g = millis() + BLINKINV;

        digitalWrite( LED_BUILTIN, (blink_g) ? HIGH : LOW );
        blink_g = !blink_g;
        }

    if ( millis() > nxtRpt_g )
        {
        nxtRpt_g = millis() + (RPTINV * 1000);

#ifdef USE_DS18
        float t;
    
        dt_g.requestTemperatures();

        t = dt_g.getTempFByIndex( 0 );

        if ( isnan( t ) || (t < 0.0) )
            {
            t = 0;
            }

        sprintf( buf, "{\"MAC\":\"%s\",\"RPTCNT\":%u,\"t\":%.1f}",
                mac_g, rptCnt_g ++, t );
#else
        sprintf( buf, "{\"MAC\":\"%s\",\"RPTCNT\":%u,\"ERR\":\"(HUH)NO SENSOR\"}",
                mac_g, rptCnt_g ++ );
#endif

#ifdef USE_ESPNOW
        quickEspNow.send( broadcast_g, (uint8_t *) buf, strlen( buf ) );
#endif
#ifdef USE_PUBSUB
#ifdef USE_WIFI
        if ( wifi_g != NULL )
            {
            ps_g.publish( logTopic_g, buf );
            }
#endif
#endif
#ifdef USE_MESH
        if ( gwAddr_g.size() == 0 )
            {
            mesh.sendBroadcast( String( buf ) );
            mesh.sendBroadcast( String( "GW" ) );
            Serial.printf( "(%d)sendBroadcast - %s\n", __LINE__, buf );
            }
        else
            {
            for( uint32_t gw : gwAddr_g )
                {
                mesh.sendSingle( gw, String( buf ) );
                Serial.printf( "(%d)sendSingle(0x%X) - %s\n", __LINE__, gw, buf );
                }
            }
#else
        Serial.println( buf );
#endif
        }

    if ( (sleepTime_g != 0) && (millis() > sleepTime_g) )
        {
        sleepTime_g = 0;
        sprintf( buf, "{\"MAC\":\"%s\",\"awake\":%lu,\"sleep\":%u,\"ID\":\"%s\"}",
                mac_g, (millis() - stTime_g)/1000, SLEEPTIME, ID );
#ifdef USE_ESPNOW
        quickEspNow.send( broadcast_g, (uint8_t *) buf, strlen( buf ) );
#endif
#ifdef USE_MESH
        if ( gwAddr_g.size() == 0 )
            {
            mesh.sendBroadcast( String( buf ) );
            mesh.sendBroadcast( String( "GW" ) );
            Serial.printf( "(%d)sendBroadcast - %s\n", __LINE__, buf );
            }
        else
            {
            for( uint32_t gw : gwAddr_g )
                {
                mesh.sendSingle( gw, String( buf ) );
                }
            Serial.printf( "(%d)sendSingle - %s\n", __LINE__, buf );
            }
#endif
        Serial.println( buf );
        digitalWrite( SENSORPWR, LOW );
        delay( 1000 );

#ifdef ESP32
        esp_sleep_enable_timer_wakeup( SLEEPTIME * 1e6 );
        delay( 100 );
        esp_deep_sleep_start();
#else
        ESP.deepSleep( SLEEPTIME * 1e6 );
#endif
        }

    yield();
    }

