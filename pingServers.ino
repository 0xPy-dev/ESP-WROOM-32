#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_wifi.h"
#include <ESP32Ping.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include "SPIFFS.h"

#define BOT_TOKEN "******:******"
#define CHAT_ID "******"
#define DELAY 30000
#define LED 19

const char* ssid     = "******";
const char* password = "******";
//byte mac[] = { 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX };

const char* ntpServer = "Your NTP POOL";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

String remote_hosts[256] = {};
String remote_hosts_info[256] = {};
int down_hosts_id_list[] = {};

const int count = 3;
const int err_count = 5;

int cdh = 0;
int len_array = 0;

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

void start() {
  Serial.println("START");
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
}

void quit() {
  Serial.println("STOP");
  Serial.println();
  digitalWrite(LED, LOW);
}

int count_lines(String s) {
  int x = 0;
  int a = 0;
  
  while (s.indexOf('\n') >= 0) {
    a = s.indexOf('\n');
    
    if (a != -1) {
      x++;
    }
    s = s.substring(a+1);
  }

  return(x);
}

int split_list(String text, char symbol) {
  int n = 0;
  int p = 0;
  int i = 0;
  int c_lines = count_lines(text);
  
  for (;i<c_lines;i++) {
    p = text.indexOf(symbol); // p = 5
    remote_hosts[i] = text.substring(0, p);
    text = text.substring(p+1);

    p = text.indexOf('\n'); // p = 4
    remote_hosts_info[i] = text.substring(0, p);
    text = text.substring(p+1);
  }
  
  return(i);
}

String input() {
  while (true) {
    String buff = Serial.readString();
    buff = buff.substring(0, buff.length() - 1);

    if (buff.length() > 0) {
      return (buff);
    }
  }
}

void get_ip_list() {
  HTTPClient http;
  String url = "https://raw.githubusercontent.com/0xPy-dev/ESP-WROOM-32/main/data/ip.lst";
  
  http.begin(url.c_str());
  int ResponseCode = http.GET();
  
  if (ResponseCode > 0) {
    len_array = split_list(http.getString(), ' ');  
    http.end();
  }
}

void read_ip_list() {
  File file = SPIFFS.open("/ip.lst", "r");
  
  if (!file) {
    Serial.println("Error opening file");
    
    while (true) {
      Serial.print("Get ip.lst file for another source?[yes/no]: ");
      String output = input();
      
      if (output == "yes") {
        get_ip_list();
      }
      else if (output == "no") {
        ESP.restart();
      }
      else {
        continue;
      }
    }
    
    return;
  }
  else {
    len_array = split_list(file.readString(), ' ');
  }
}

String human_view(int value) {
  int level = 0;
  String scale_bytes[] = {"B", "K", "M", "G"};
  
  while (value/1024 >= 1000) {
    value /= 1024;
    level++;
  }
  return(String(value)+scale_bytes[level]);
}

void show_fsstat(String human = "") {
  String total, used, free;
  
  if (human != "") {
    total = human_view(SPIFFS.totalBytes());
    used  = human_view(SPIFFS.usedBytes());
    free  = human_view(SPIFFS.totalBytes() - SPIFFS.usedBytes());
  }
  else {
    total = String(SPIFFS.totalBytes());
    used  = String(SPIFFS.usedBytes());
    free  = String(SPIFFS.totalBytes() - SPIFFS.usedBytes());
  }
  int procent = (SPIFFS.usedBytes() / SPIFFS.totalBytes()) * 100;
  if (procent == 0) procent++;
  
  Serial.println("Файловая система ESP32\tВсего\tИспользовано\tСвободно\tИспользовано%"); // HEADER
  Serial.print  ("/\t\t\t" + total);
  Serial.print  ("\t"  + used);
  Serial.print  ("\t\t"  + free);
  Serial.println("\t\t"  + String(procent));
}

void show_file(String path) {
  SPIFFS.begin(true);

  if (path[0] != '/') {
    path = "/" + path;
  }

  File file = SPIFFS.open(path, "r");
  String content = file.readString();
  Serial.println(content);
  file.close();
}

void del_file(String path) {
  SPIFFS.begin(true);
  
  if (path[0] != '/') {
    path = "/" + path;
  }
  
  SPIFFS.remove(path);
}

void list_dir(String path="/") {
  SPIFFS.begin(true);
  File dir = SPIFFS.open(path);
  File file = dir.openNextFile();

  while (file) {
    if (file.isDirectory()) {
      Serial.print("\t" + String(file.size()));
      Serial.println("\t<DIR> \t" + String(file.name()) + "/");
    }
    else {
      Serial.print("\t" + String(file.size()));
      Serial.println("\t<FILE>\t" + String(file.name()));
    }
    
    delay(1000);
    file = dir.openNextFile();
  }
}

void help() {
  Serial.println("help");
}

String split(String text, char delimiter, int index_of) {
  int i = 0;
  int len_buff = 32;
  String buff[len_buff];
  
  for (auto symbol: text) if (symbol == delimiter) {
    int p = text.indexOf(delimiter);
    
    if (i < len_buff) {
      buff[i] = text.substring(0, p);
      text = text.substring(p+1);
      i++;
    }
    else {
      Serial.println("Error write, buffer overflow!");
      break;
    }
  } 
  else {
    buff[i] = text;
  }
    
  return(buff[index_of]);
}

void handle_input(String output) {
  int i = 0;
  String res = split(output, ' ', i);
  
  if (res == "ls") {
    i += 1;
    String argument = String(split(output, ' ', i));
    
    if (argument != "") {
      return(list_dir(argument));
    }
    else list_dir();
  }
  else if (res == "del") {
    i += 1;
    return(del_file(split(output, ' ', i)));
  }
  else if (res == "show") {
    i += 1;
    return(show_file(split(output, ' ', i)));
  }
  else if (res == "fsstat") {
    i += 1;
    return(show_fsstat(split(output, ' ', i)));
  }
  else if (res == "help") {
    return(help());
  }
  else {
    if (res == "quit") return;
    else return(help());
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  start();
  
  IPAddress local_ip(192, 168, 48, 106);
  IPAddress gateway(192, 168, 48, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(8, 8, 4, 4);
  WiFi.config(local_ip, gateway, subnet, dns1, dns2);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);  
  WiFi.begin(ssid, password);
  delay(500);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  delay(100);
  Serial.print("WIFI connected with IP: ");
  Serial.println(WiFi.localIP());
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  SPIFFS.begin(true);
  delay(100);
  read_ip_list();
}

void loop() {
  start();
  int err = 0;
  
  for (int index=0;index<len_array;index++) {
    if (Serial.available()) {
      String output;
      Serial.println("#Переключение в интерактивный режим");
      Serial.println("#Для выхода введите команду: quit");
      
      while (output != "quit") {
        Serial.print("interactive mode:~> ");
        output = input();
        Serial.println(output);
        
        handle_input(output);
      }
    }
    
    char ip[remote_hosts[index].length() + 1];
    strcpy(ip, remote_hosts[index].c_str());
    int len_ip = strlen(ip);
    int n = 0;
    String sublst[] = {"", "", "", ""};
    bool res = false;
    
    for (int i=0;i<len_ip;i++) {      
      if (isDigit(ip[i])) {
        sublst[n] += ip[i];
      }
      else if (ip[i] == '.') {
        n++;
        continue;
      }
      else {
        n = -1;
        break;
      }
    }

    if (n > 0) {
      Serial.print("PING CURRENT HOST: ");
      Serial.println(ip);
      res = Ping.ping(IPAddress(
        sublst[0].toInt(),
        sublst[1].toInt(),
        sublst[2].toInt(),
        sublst[3].toInt()
      ), count);
      float avg_time_ms = Ping.averageTime();
      Serial.print("Среднее время ответа: ");
      Serial.println(avg_time_ms);
    }
    else {
      Serial.print("PING CURRENT HOST: ");
      Serial.println(ip);
      res = Ping.ping(ip, count);
      float avg_time_ms = Ping.averageTime();
      Serial.print("Среднее время ответа: ");
      Serial.println(avg_time_ms);
    }
    if (not res) {
      if (err < err_count) {
        err++;
        index--;
        continue;
      }
    }
    else {
      bool flag = false;
      int l;
      
      for (l=0;l<cdh;l++) {
        if (down_hosts_id_list[l] == index) {
          flag = true;
          break;
        }
      }
      
      if (flag) {
        down_hosts_id_list[l] = down_hosts_id_list[cdh];
        cdh -= 1;
        String info = remote_hosts_info[index];
        String msg  = "🟢 HOST IS UP: [ " + String(ip) + "]\nINFO BY HOST: [" + info + "]";
        Serial.print("IP IS VALID: ");
        Serial.println(ip);
        bot.sendMessage(CHAT_ID, msg, "");
      }
    }

    if (err >= 5) {     
      bool flag = true;
      err = 0;
      
      for (int l=0;l<cdh;l++) {
        if (down_hosts_id_list[l] == index) {
          flag = false;
          break;
        }
      }
      
      if (flag == true) {
        down_hosts_id_list[cdh] = index;
        cdh += 1;
        String info = remote_hosts_info[index];
        String msg = "🔴 HOST IS DOWN: [ " + String(ip) + "]\nINFO BY HOST: [" + info + "]";
        Serial.print("IP IS NOT VALID: ");
        Serial.println(ip);
        bot.sendMessage(CHAT_ID, msg, "");
      }
    }
  }

  quit();
  delay(DELAY);
}
