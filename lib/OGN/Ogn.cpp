/*!
 * @file Ogn.cpp
 *
 *
 */

#include "Ogn.h"

Ogn::Ogn(){
    _user = "";
    client = NULL;
    xMutex = NULL;
}

void Ogn::setClient(Client *_client){
    client = _client;
}

void Ogn::setMutex(SemaphoreHandle_t *_xMutex){
    xMutex = _xMutex;
}

bool Ogn::begin(String user,String version){
    //_version = "v0.0.1.GX";  //version;
    _version = version;
    //_user = "FNB" + user; //base-station
    _user = user; //base-station
    connected = false;
    GPSOK = 0;
    tStatus = 0;
    if (client == NULL){ //if now client is set, we use the wifi-client
      client = new WiFiClient();
    }
    if (xMutex == NULL){ //create new Mutex
        xMutex = new SemaphoreHandle_t();
        *xMutex = xSemaphoreCreateMutex();
    }
    return true;
}

void Ogn::end(void){

}

void Ogn::setAirMode(bool _AirMode){
    AirMode = _AirMode;
}

void Ogn::sendLoginMsg(void){
    //String login ="user " + _user + " pass " + calcPass(_user) + " vers " + _version + " filter m/10\r\n";
    String login ="user " + _user + " pass " + calcPass(_user) + " vers " + _version + "\r\n";
    log_i("%s",login.c_str());
    xSemaphoreTake( *xMutex, portMAX_DELAY );
    client->print(login);
    xSemaphoreGive( *xMutex );
}

String Ogn::calcPass(String user){
    const int length = user.length();
    uint8_t buffer[length];
    String myString = user.substring(0,10);
    myString.toUpperCase();
    memcpy(buffer, myString.c_str(), length);
    uint16_t hash = 0x73e2;
    int i = 0;
    while (i < length){
        hash ^= buffer[i] << 8;
        if ((i+1) < length){
            hash ^= buffer[i+1];
        }
        i += 2;
    }
    hash &= 0x7fff;
    return String(hash);
}

void Ogn::connect2Server(uint32_t tAct){
    static uint32_t tConnRetry = tAct;
    initOk = 0;
    connected = false;
    if ((tAct - tConnRetry) >= 5000){
        tConnRetry = tAct;
        xSemaphoreTake( *xMutex, portMAX_DELAY );
        client->stop();
        xSemaphoreGive( *xMutex );
        //sntp_setoperatingmode(SNTP_OPMODE_POLL);
        //sntp_setservername(0, "pool.ntp.org");
        //sntp_init();
        //init and get the time
        configTime(0, 0, "pool.ntp.org");
        xSemaphoreTake( *xMutex, portMAX_DELAY );
        int ret = client->connect("aprs.glidernet.org", 14580);
        xSemaphoreGive( *xMutex );
        if (ret) {
            sendLoginMsg();
            connected = true;
        }
        
    }
}

void Ogn::checkLine(String line){
    String s = "";
    int32_t pos = 0;
    //log_i("%s",line.c_str());
    if (!line.startsWith("# aprsc")){
      log_i("%s",line.c_str());
    }
    if (initOk == 0){
        pos = getStringValue(line,"server ","\r\n",0,&s);
        if (pos > 0){
            tStatus = 0;
            tRecBaecon = millis() - OGNSTATUSINTERVALL;
            _servername = s;
            initOk = 1;
        }
    }
}

void Ogn::setGPS(float lat,float lon,float alt,float speed,float heading){
    _lat = lat;
    _lon = lon;
    _alt = alt;
    _speed = speed;
    _heading = heading;
    GPSOK = 1;
}

uint8_t Ogn::getAddressType(String devId){
    //if (devId.startsWith("08") || devId.startsWith("11") || devId.startsWith("20") || devId.startsWith("DD") || devId.startsWith("DE") || devId.startsWith("DF")){
    if (devId.startsWith("11") || devId.startsWith("20") || devId.startsWith("DD") || devId.startsWith("DE") || devId.startsWith("DF")){
        return 2; //address type FLARM
        //log_i("address=%s Flarm",devId.c_str());
    }else{
        return 3; //address type OGN        
        //log_i("address=%s OGN",devId.c_str());
    }

}

uint8_t Ogn::getSenderDetails(bool onlinetracking,aircraft_t aircraftType,String devId){
    uint8_t type = 0;
    switch (aircraftType)
    {
    case paraglider:
        type = 7;
        break;
    case hangglider:
        type = 6;
        break;
    case balloon:
        type = 11;
        break;
    case glider:
        type = 1;
        break;
    case poweredAircraft:
        type = 8;
        break;
    case helicopter:
        type = 3;
        break;
    case uav:
        type = 13;
        break;
    default:
        type = 15;
        break;
    }
    type = type << 2;
    if (!onlinetracking){
        type += 0x80; //first stealth mode boolean (should never be "1")
    }
    //cause, GXAircom is also able to send legacy-mode --> we send always as flarm-device.
    //address-type
    //  RANDOM(0) - changing (random) address generated by the device
    //	ICAO(1)
    //	FLARM(2)  FLARM HW
    //	OGN(3)    OGN tracker HW
    
    type += getAddressType(devId);
    return type;
}

String Ogn::getOrigin(String devId){
    if (getAddressType(devId) == 2){
        return "FLR";
    }else{
        return "FNT";
    }
}

void Ogn::sendNameData(String devId,String name,float snr){
    if (initOk < 10) return; //nothing todo
    String sTime = getActTimeString();
    if (sTime.length() <= 0) return;
    char buff[200];
    sprintf (buff,"%s%s>OGNFNT,qAS,%s:>%sh Name=\"%s\" %0.1fdB\r\n"
    ,getOrigin(devId).c_str(),devId.c_str(),_user.c_str(),sTime.c_str(),name.c_str(),snr);
    xSemaphoreTake( *xMutex, portMAX_DELAY );
    client->print(buff);                
    xSemaphoreGive( *xMutex );
    //log_i("%s",buff);

}

void Ogn::sendWeatherData(float lat,float lon,String devId,float wDir,float wSpeed,float wGust,float temp,float rain1h, float rain24h,float hum,float press,float snr){
    if (initOk < 10) return; //nothing todo
    float lLat = abs(lat);
    float lLon = abs(lon);
    int latDeg = int(lLat);
    int latMin = (roundf((lLat - int(lLat)) * 60 * 1000));
    int lonDeg = int(lLon);
    int lonMin = (roundf((lLon - int(lLon)) * 60 * 1000));
    int mHum = (int)hum;
    if (mHum >= 100){ //humidity 100% --> 00
        mHum = 0;
    }else if (mHum == 0){
        mHum = 1;
    }
    String sTime = getActTimeString();
    if (sTime.length() <= 0) return;
    char buff[200];
    //sprintf (buff,"%s%s>OGNFNT,qAS,%s:/%sh%02d%02d.%02d%c/%03d%02d.%02d%c_%03d/%03dg%03dt%03dr%03dp%03dh%02db%05d %0.1fdB\r\n"
    //,getOrigin(devId).c_str(),devId.c_str(),_user.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(lon < 0)?'W':'E',
    //int(wDir),int(kmh2mph(wSpeed)),int(kmh2mph(wGust)),int(deg2f(temp)),int(rain1h * 10),int(rain24h * 10),mHum,int(press * 10),snr);
    String send = "";

    //sprintf (buff,"%s%s>OGNFNT,qAS,%s:/%sh%02d%02d.%02d%c/%03d%02d.%02d%c_%03d/%03dg%03dt%03d"
    //,getOrigin(devId).c_str(),devId.c_str(),_user.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(lon < 0)?'W':'E',
    sprintf (buff,"FNT%s>OGNFNT,qAS,%s:/%sh%02d%02d.%02d%c/%03d%02d.%02d%c_%03d/%03dg%03dt%03d"
    ,devId.c_str(),_user.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(lon < 0)?'W':'E',
    int(wDir),int(kmh2mph(wSpeed)),int(kmh2mph(wGust)),int(deg2f(temp)));
    send += buff;
    if (rain1h != NAN){
        sprintf (buff,"r%03dp%03d"
        ,int(rain1h * 10),int(rain24h * 10));
        send += buff;
    }
    sprintf (buff,"h%02db%05d %0.1fdB\r\n"
    ,mHum,int(press * 10),snr);
    send += buff;


    xSemaphoreTake( *xMutex, portMAX_DELAY );
    client->print(send.c_str());                
    xSemaphoreGive( *xMutex );
    //log_i("%s",send.c_str());

}

void Ogn::sendGroundTrackingData(float lat,float lon,String devId,uint8_t state,float snr){
    if (initOk < 10) return; //nothing todo
    char buff[200];
    float lLat = abs(lat);
    float lLon = abs(lon);
    int latDeg = int(lLat);
    int latMin = (roundf((lLat - int(lLat)) * 60 * 1000));
    int lonDeg = int(lLon);
    int lonMin = (roundf((lLon - int(lLon)) * 60 * 1000));
    String sTime = getActTimeString();
    if (sTime.length() <= 0) return;

    //sprintf (buff,"%s%s>APRS,qAR:/%sh%02d%02d.%02d%c\\%03d%02d.%02d%cn !W%01d%01d! id%02X%s FNT%d %0.1fdB\r\n"
    //            ,getOrigin(devId).c_str(),devId.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(lon < 0)?'W':'E',int(latMin %10),int(latMin %10),getSenderDetails(aircraftType,devId),devId.c_str(),state,snr);
    sprintf (buff,"%s%s>OGNFNT,qAS,%s:/%sh%02d%02d.%02d%c\\%03d%02d.%02d%cn !W%01d%01d! id%02X%s FNT%d %0.1fdB\r\n" //3F OGN-Tracker and device 15
    ,getOrigin(devId).c_str(),devId.c_str(),_user.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(lon < 0)?'W':'E',int(latMin %10),int(latMin %10),getSenderDetails(true,aircraft_t::unknown,devId),devId.c_str(),state,snr);
    xSemaphoreTake( *xMutex, portMAX_DELAY );
    client->print(buff);                
    xSemaphoreGive( *xMutex );
    //log_i("%s",buff);

}

void Ogn::sendTrackingData(float lat,float lon,float alt,float speed,float heading,float climb,String devId,aircraft_t aircraftType,bool Onlinetracking,float snr){
    //if ((WiFi.status() != WL_CONNECTED) || (initOk < 10)) return; //nothing todo
    if (initOk < 10) return; //nothing todo
    char buff[200];
    float lLat = abs(lat);
    float lLon = abs(lon);
    int latDeg = int(lLat);
    int latMin = (roundf((lLat - int(lLat)) * 60 * 1000));
    int lonDeg = int(lLon);
    int lonMin = (roundf((lLon - int(lLon)) * 60 * 1000));
    String sTime = getActTimeString();
    if (sTime.length() <= 0) return;

    //sprintf (buff,"%s%s>APRS,qAR:/%sh%02d%02d.%02d%c/%03d%02d.%02d%cg%03d/%03d/A=%06d !W%01d%01d! id%02X%s %+04.ffpm FNT11 %0.1fdB\r\n"
    //            ,getOrigin(devId).c_str(),devId.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(lon < 0)?'W':'E',int(heading),int(speed * 0.53996),int(alt * 3.28084),int(latMin %10),int(latMin %10),getSenderDetails(aircraftType,devId),devId.c_str(),climb*196.85f,snr);
    sprintf (buff,"%s%s>OGNFNT,qAS,%s:/%sh%02d%02d.%02d%c/%03d%02d.%02d%cg%03d/%03d/A=%06d !W%01d%01d! id%02X%s %+04.ffpm FNT11 %0.1fdB\r\n"
                ,getOrigin(devId).c_str(),devId.c_str(),_user.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(lon < 0)?'W':'E',int(heading),int(speed * 0.53996),int(alt * 3.28084),int(latMin %10),int(latMin %10),getSenderDetails(Onlinetracking,aircraftType,devId),devId.c_str(),climb*196.85f,snr);
    xSemaphoreTake( *xMutex, portMAX_DELAY );
    client->print(buff);                
    xSemaphoreGive( *xMutex );
    //log_i("%s",buff);
}

void Ogn::setStatusData(float pressure, float temp,float hum, float battVoltage){
    _Pressure = pressure;
    _Temp = temp;
    _Hum = hum;
    _BattVoltage = battVoltage;
}

void Ogn::sendReceiverStatus(String sTime){
    //String sStatus = _user + ">APRS,TCPIP*,qAC," + _servername + ":>" + sTime + "h h00 " + _version + "\r\n";
    String sStatus = _user + ">OGNFNT,TCPIP*,qAC," + _servername + ":>" + sTime + "h " + _version + " ";
    if (!isnan(_alt)){
        sStatus += String(_alt,0) + "m "; //send altitude
    }
    if (!isnan(_Pressure)){
        sStatus += String(_Pressure,1) + "hPa "; //send pressure
    }
    if (!isnan(_Temp)){
        if (_Temp >= 0){
            sStatus += "+";
        }
        sStatus += String(_Temp,1) + "C "; //send Temp
    }
    if (!isnan(_Hum)){
        sStatus += String(_Hum,0) + "% "; //send humidity
    }
    if (!isnan(_BattVoltage)){
        sStatus += String(_BattVoltage,2) + "V "; //send batt-voltage
    }
    sStatus += "\r\n";
    xSemaphoreTake( *xMutex, portMAX_DELAY );
    client->print(sStatus);
    xSemaphoreGive( *xMutex );
    //log_i("%s",sStatus.c_str());
}

void Ogn::sendReceiverBeacon(String sTime){
    char buff[200];
    float lLat = abs(_lat);
    float lLon = abs(_lon);
    int latDeg = int(lLat);
    int latMin = (roundf((lLat - int(lLat)) * 60 * 1000));
    int lonDeg = int(lLon);
    int lonMin = (roundf((lLon - int(lLon)) * 60 * 1000));

    if (AirMode){
        sprintf (buff,"%s>OGNFNT,TCPIP*,qAC,%s:/%sh%02d%02d.%02d%cI%03d%02d.%02d%c&%03d/%03d/A=%06d !W%01d%01d!\r\n"
                    ,_user.c_str(),_servername.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(_lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(_lon < 0)?'W':'E',int(_heading),int(_speed * 0.53996),int(_alt * 3.28084),int(latMin %10),int(latMin %10));
    }else{
        sprintf (buff,"%s>OGNFNT,TCPIP*,qAC,%s:/%sh%02d%02d.%02d%cI%03d%02d.%02d%c&/A=%06d\r\n"
                    ,_user.c_str(),_servername.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(_lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(_lon < 0)?'W':'E',int(_alt * 3.28084));
    }
    //sprintf (buff,"%s>APRS,TCPIP*,qAC,%s:/%sh%02d%02d.%02d%cI%03d%02d.%02d%c&%03d/%03d/A=%06d !W%01d%01d!\r\n"
    //            ,_user.c_str(),_servername.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(_lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(_lon < 0)?'W':'E',int(_heading),int(_speed * 0.53996),int(_alt * 3.28084),int(latMin %10),int(latMin %10));

    //sprintf (buff,"%s>APRS,TCPIP*,qAC,%s:/%sh%02d%02d.%02d%cI%03d%02d.%02d%c&%03d/%03d/A=%06d\r\n"
    //            ,_user.c_str(),_servername.c_str(),sTime.c_str(),latDeg,latMin/1000,latMin/10 %100,(_lat < 0)?'S':'N',lonDeg,lonMin/1000,lonMin/10 %100,(_lon < 0)?'W':'E',int(_heading),int(_speed * 0.53996),int(_alt * 3.28084));
    //log_i("%s",buff);
    xSemaphoreTake( *xMutex, portMAX_DELAY );
    client->print(buff); 
    xSemaphoreGive( *xMutex );
    initOk = 10; //now we can send, because we have sent GPS-Position               
    
}

String Ogn::getActTimeString(){
    struct tm timeinfo;
    char buff[20];
    if (timeStatus() == timeSet){
        //log_i("%d %d %d %d:%d:%d",year(),month(),day(),hour(),minute(),second());
        sprintf (buff,"%02d%02d%02d",hour(),minute(),second());
        //log_i("%s",buff);
        return String(buff);
    }else{
        return "";
    }
    /*
    if(getLocalTime(&timeinfo)){
        strftime(buff, 20, "%H%M%S", &timeinfo);
        return String(buff);
    }else{
        return "";
    }
    */
}

void Ogn::sendStatus(uint32_t tAct){
    if (initOk > 0){        
        if (GPSOK){
            if ((tAct - tRecBaecon) >= OGNSTATUSINTERVALL){
                tRecBaecon = tAct;
                tStatus = tAct;
                String sTime = getActTimeString();
                if (sTime.length() > 0){
                    sendReceiverBeacon(sTime);
                    //sendReceiverStatus(sTime);
                }
            }
        }
        //we have to send ReceiverBeacon and afterwards the status (Receiverbaecon always first)
        if (((tAct - tStatus) >= 5000) && (tStatus != 0)){
            String sTime = getActTimeString();
            if (sTime.length() > 0){
                sendReceiverStatus(sTime);
            }
            tStatus = 0;
        }
    }
}

void Ogn::readClient(){
  String line = "";
  xSemaphoreTake( *xMutex, portMAX_DELAY );
  while (client->available()){
    char c = client->read(); //read 1 character 
    
    line += c; //read 1 character
    if (c == '\n'){
        checkLine(line);
        line = "";
    }

  }
  xSemaphoreGive( *xMutex );
}

void Ogn::checkClientConnected(uint32_t tAct){
    static uint32_t tCheck = tAct;
    if ((tAct - tCheck) >= 10000){
        tCheck = tAct;
        xSemaphoreTake( *xMutex, portMAX_DELAY );
        if (!client->connected()){
            connected = false;        
        }
        xSemaphoreGive( *xMutex );
    }
}

void Ogn::run(bool bNetworkOk){    
    uint32_t tAct = millis();    
    //if (WiFi.status() == WL_CONNECTED){
    if (bNetworkOk){
        checkClientConnected(tAct);
        if (connected){
            readClient();
            sendStatus(tAct);
        }else{
            //not connected --> try to connect
            connect2Server(tAct);
        }
    }else{
        connected = false;
    }
}