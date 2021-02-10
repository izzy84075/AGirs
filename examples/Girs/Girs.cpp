/*
Copyright (C) 2014,2015,2017 Bengt Martensson.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see http://www.gnu.org/licenses/.
*/

#include "config.h"
#include "GirsLib/LedLcdManager.h"
#include "GirsLib/GirsUtils.h"
#include "GirsLib/StreamParser.h"

#ifdef ARDUINO
#include <avr/pgmspace.h>
#else
#define PROGMEM
#include <cstring>
#include <iostream>
#include <sstream>
#endif

// Conditional includes
#if defined(ETHERNET) & !defined(ARDUINO)
#error not supported
#endif

#ifdef ETHERNET
#include <Ethernet.h>
#include <IPAddress.h>
#endif // ETHERNET

#ifdef LCD
#include <LiquidCrystal_I2C.h>
#endif

#ifdef RECEIVE
#include <IrReceiverSampler.h>
#endif

#ifdef TRANSMIT
#include <IrSenderPwm.h>
#endif
#if defined(RECEIVE)
#include <IrReceiverSampler.h>
#endif

#ifdef CAPTURE
#include <IrWidgetAggregating.h>
#endif

#ifdef NON_MOD
#include <IrSenderNonMod.h>
#endif

#ifdef DECODER
#ifndef RECEIVE
#error DECODER without RECEIVE is nonsensical, aborting.
#endif
#include <MultiDecoder.h>
#endif

#ifdef RENDERER
#ifndef TRANSMIT
#error RENDER without TRANSMIT is nonsensical, aborting.
#endif
#include <IrSignal.h>
#include <Nec1Renderer.h>
#include <Rc5Renderer.h>
#endif

#ifdef NAMED_COMMANDS
#error NAMED_COMMANDS is presently not supported
#endif

#ifdef BEACON
#include <Beacon.h>
#endif

#ifdef PRONTO
#include <Pronto.h>
#endif

#if defined(CONFIGURABLE_LEDS) & (! defined(PARAMETERS) | !defined(LED))
#error CONFIGURABLE_LEDS defined but not PARAMETERS and LED, aborting.
#endif

#ifdef CONFIGURABLE_LEDS
#define LED_PARAMETER_CONST
#else
#define LED_PARAMETER_CONST const
#endif

#ifdef PARAMETERS
#define PARAMETERS_NAME Parameters
#define PARAMETER_CONST
#else
#define PARAMETERS_NAME
#define PARAMETER_CONST const
#endif

#ifdef LED
#ifdef TRANSMITLED
static LED_PARAMETER_CONST led_t transmitled = TRANSMITLED;
#endif

#ifdef RECEIVELED
static LED_PARAMETER_CONST led_t receiveled = RECEIVELED;
#endif

#ifdef CAPTURELED
static LED_PARAMETER_CONST led_t captureled = CAPTURELED;
#endif

#ifdef COMMANDLED
static LED_PARAMETER_CONST led_t commandled = COMMANDLED;
#endif
#endif // LED

#ifdef CAPTURE
static PARAMETER_CONST unsigned long captureEndingTimeout = DEFAULT_CAPTURE_ENDINGTIMEOUT; // milliseconds
static const unsigned sensorNo = 1; // presently, only one supported
#endif
#ifdef RECEIVE
static PARAMETER_CONST unsigned long receiveEndingTimeout = DEFAULT_RECEIVE_ENDINGTIMEOUT; // milliseconds
#endif
#if defined(RECEIVE) | defined(CAPTURE)
static PARAMETER_CONST unsigned long beginTimeout = DEFAULT_BEGINTIMEOUT; // milliseconds
static PARAMETER_CONST uint16_t captureSize = DEFAULT_CAPTURESIZE;
#endif

#ifdef RECEIVE
#ifdef IRRECEIVER_2_PIN
static PARAMETER_CONST uint8_t receiverNo = 1;
#else
static const uint8_t receiverNo = 1;
#endif
#endif

#ifdef ETHERNET
EthernetServer server(PORT);
#endif // ETHERNET

#ifdef RESET
bool reset = false;
#endif

#define modulesSupported EXPAND_AND_QUOTE(Base TRANSMIT_NAME CAPTURE_NAME RENDERER_NAME RECEIVE_NAME DECODER_NAME LED_NAME LCD_NAME PARAMETERS_NAME NAMED_COMMANDS_NAME PRONTO_NAME)
#ifndef PROGNAME
#define PROGNAME "AGirs"
#endif
#ifndef VERSION
#include "GirsLib/version.h"
#endif // VERSION
#define okString "OK"
#define errorString "ERROR"
#define timeoutString "."

static constexpr size_t cmdLength = 20U;

#ifdef TRANSMIT

static bool sendIrSignal(const IrSignal &irSignal, unsigned int noSends=1) {
    if (noSends == 0)
        return false;
#ifdef TRANSMITLED
    LedLcdManager::setLogicLed(transmitled, LedLcdManager::on);
#endif
    IrSender *irSender =
#ifdef NON_MOD
            (irSignal.getFrequency() == 0) ? (IrSender*) new IrSenderNonMod(NON_MOD_PIN) :
#endif
            (IrSender*) IrSenderPwm::getInstance(true);

    irSender->sendIrSignal(irSignal, noSends);

#ifdef NON_MOD
    if (irSignal.getFrequency() == 0)
        delete irSender;
    else
#endif
        IrSenderPwm::deleteInstance();

#ifdef TRANSMITLED
    LedLcdManager::setLogicLed(transmitled, LedLcdManager::off);
#endif
    return true;
}

#endif // TRANSMIT

# if defined(RECEIVE) | defined(CAPTURE)
static void flushIn(Stream &stream UNUSED) {
#ifdef ARDUINO
    while (stream.available())
        stream.read();
#endif
}
#endif

#ifdef RECEIVE

static void decodeOrDump(IrReader *irReader, Stream& stream) {
#ifdef DECODER
    MultiDecoder multiDecoder(*irReader);
#ifdef LCD
    if (multiDecoder.getType() > MultiDecoder::noise) {
        LedLcdManager::lcdPrint(multiDecoder.getType() == MultiDecoder::nec_ditto
                ? "." : multiDecoder.getDecode(),
                multiDecoder.getType() != MultiDecoder::nec_ditto);
        if (multiDecoder.getType() == MultiDecoder::nec)
            LedLcdManager::lcdSetCursor(0, 1); // prepare for dittos
    }
#endif

#ifdef DECODELED
    LedLcdManager::setLogicLed(DECODELED(multiDecoder.getType()), LedLcdManager::blink);
#endif
#endif

#if defined(DECODER) & ! defined(DONT_REPORT_DECODES) // lircd does its own decoding
    switch (multiDecoder.getType()) {
        case MultiDecoder::noise:
            // ignore
            break;
        case MultiDecoder::undecoded:
            irReader->dump(stream); // report data of undecoded signals
            break;
        default:
            stream.println(multiDecoder.getDecode()); // also for timeout
            break;
    }
#else  // ! (defined(DECODER) & ! defined(DONT_REPORT_DECODES))
    if (irReader->isEmpty())
        stream.println(F(timeoutString));
    else
        irReader->dump(stream);
#endif // !DECODER
}

static bool receive(StreamParser& parser) {
    IrReceiverSampler *irReceiver = IrReceiverSampler::getInstance();
    if (irReceiver == nullptr)
        irReceiver = IrReceiverSampler::newIrReceiverSampler(captureSize,
                GirsUtils::receiverPin(receiverNo), GirsUtils::receiverPullup(receiverNo));
    if (irReceiver == nullptr)
        return false;
    irReceiver->setEndingTimeout(receiveEndingTimeout);
    irReceiver->setBeginningTimeout(beginTimeout);
    irReceiver->setMarkExcess(IRRECEIVER_MARK_EXCESS);
    //Stream& stream = parser.getStream();
#ifdef ARDUINO
    flushIn(parser.getStream());
#endif
#ifdef RECEIVELED
    LedLcdManager::setLogicLed(receiveled, LedLcdManager::on);
#endif
    irReceiver->enable();
#ifdef ARDUINO
    while (!irReceiver->isReady() && stream.available() == 0)
#else
        std::cout << "** Simulating timeout **" << std::endl;
        delay(beginTimeout);
#endif
        LedLcdManager::checkTurnoff();
    bool ready = irReceiver->isReady();
    irReceiver->disable();
#ifdef RECEIVELED
     LedLcdManager::setLogicLed(receiveled, LedLcdManager::off);
#endif
     if (ready)
         decodeOrDump(irReceiver, stream);
     IrReceiverSampler::deleteInstance();
     return true;
}
#endif // RECEIVE

#ifdef CAPTURE

static bool capture(Stream& stream) {
    IrWidget *irWidget = IrWidgetAggregating::newIrWidgetAggregating(captureSize,
            GirsUtils::sensorPullup(sensorNo));
    if (irWidget == nullptr)
        stream.println(F("This cannot happen"));
    irWidget->setEndingTimeout(captureEndingTimeout);
    irWidget->setBeginningTimeout(beginTimeout);
    irWidget->setMarkExcess(IRSENSOR_MARK_EXCESS);
    irWidget->reset();
#ifdef CAPTURELED
    LedLcdManager::setLogicLed(captureled, LedLcdManager::on);
#endif
    flushIn(stream);
#ifdef ARDUINO
    irWidget->capture();
#else
    std::cout << "** Simulating timeout **" << std::endl;
    delay(beginTimeout);
#endif

#ifdef CAPTURELED
    LedLcdManager::setLogicLed(captureled, LedLcdManager::off);
#endif
    if (!irWidget->isEmpty()) {
        // Trying to decode the capture does not make sense,
        // that is what "receive" is for.
        irWidget->dump(stream);
    } else
        stream.println(F(timeoutString));
    IrWidgetAggregating::deleteInstance();
    return true;
}
#endif // CAPTURE

#ifdef NAMED_COMMANDS
// Defines a const IrNamedRemoteSet remoteSet with commands to be used.
//#include "my_named_remotes.inc"
extern const IrNamedRemoteSet remoteSet;

static bool sendNamedCommand(Stream& stream, String& remoteName, String& commandName, unsigned int noSends) {
    const IrNamedRemote* remote = remoteSet.getIrNamedRemote(remoteName.c_str());
    if (remote == nullptr) {
        stream.println(F("No such remote"));
        return false;
    }

    const IrNamedCommand* command = remote->getIrNamedCommand(commandName.c_str());
    if (command == nullptr) {
        stream.println(F("No such command"));
        return false;
    }
    const IrSignal *irSignal = command->getIrSignal();
    bool status = sendIrSignal(*irSignal, noSends); // waits, blinks
    delete irSignal;
    return status;
}

static void dumpRemote(Stream& stream, String& name) {
    if (name.length() == 0) {
        for (unsigned int i = 0; i < remoteSet.getNoIrNamedRemotes(); i++) {
            stream.print(remoteSet.getIrNamedRemotes()[i]->getName());
            stream.print(" ");
        }
        stream.println();
    } else {
        const IrNamedRemote* remote = remoteSet.getIrNamedRemote(name.c_str());
        if (remote == nullptr)
            stream.println(F("No such remote"));
        else {
            for (unsigned int i = 0; i < remote->getNoCommands(); i++) {
                stream.print(remote->getAllCommands()[i]->getName());
                stream.print(" ");
            }
            stream.println();
        }
    }
}
#endif

void setup() {
    LedLcdManager::setupLedGroundPins();
    GirsUtils::setupReceivers();
    GirsUtils::setupSensors();
#if defined(TRANSMIT)
    // Make sure that sender is quiet (if reset or such)
    IrSenderPwm::getInstance(true)->mute();
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    LedLcdManager::setup(LCD_I2C_ADDRESS, LCD_WIDTH, LCD_HEIGHT,
            (const pin_t[]) {SIGNAL_LED_1, SIGNAL_LED_2, SIGNAL_LED_3, SIGNAL_LED_4,
                    SIGNAL_LED_5, SIGNAL_LED_6, SIGNAL_LED_7, SIGNAL_LED_8 });
    LedLcdManager::selfTest(F(PROGNAME "\n" VERSION));
#pragma GCC diagnostic pop
#ifdef LED
    LedLcdManager::setupShouldTimeout(transmitled, false);
    LedLcdManager::setupShouldTimeout(receiveled, false);
    LedLcdManager::setupShouldTimeout(captureled, false);
    LedLcdManager::setupShouldTimeout(commandled, false);
#endif
#ifdef LCD
#ifdef ETHERNET
    LedLcdManager::lcdPrint(F("TCP"), false, 0, 2);
#ifdef SERIAL_DEBUG
    LedLcdManager::lcdPrint(F(",SerialDbg"), false);
#endif
#else // ! ETHERNET
    LedLcdManager::lcdPrint(F("Serial"), false, 0, 2);
#endif // ! ETHERNET
#endif // LCD

#ifdef ETHERNET
#ifdef SDCARD_ON_ETHERSHIELD_PIN
    // disable the SD card, as recommended in the doc
    pinMode(SDCARD_ON_ETHERSHIELD_PIN, OUTPUT);
    digitalWrite(SDCARD_ON_ETHERSHIELD_PIN, LOW);
#endif
    byte mac[] = { MACADDRESS };
#ifdef DHCP
    Ethernet.begin(mac);
#else // !DHCP
    Ethernet.begin(mac, IPAddress(IPADDRESS), IPAddress(DNSSERVER), IPAddress(GATEWAY), IPAddress(SUBNETMASK));
#endif // !DHCP
    char ipstring[16];
    sprintf(ipstring, "%d.%d.%d.%d", Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);
    LedLcdManager::lcdPrint(ipstring, false, 0, 3);

#ifdef BEACON
    Beacon::setup(PROGNAME, "DE-AD-BE-EF-FE-ED", "Utility", "www.harctoolbox.org",
            "", "", "", "http://arduino/nosuchfile.html");
#endif

    server.begin();

#endif // ETHERNET

#if defined(ARDUINO) & !defined(ETHERNET) | defined(SERIAL_DEBUG)

    Serial.begin(SERIALBAUD);
    while (!Serial)
        ; // wait for serial port to connect. "Needed for Leonardo only"

    Serial.println(F(PROGNAME " " VERSION));
    Serial.setTimeout(SERIALTIMEOUT);

#ifdef ETHERNET
    Serial.println(Ethernet.localIP());
#endif
#endif // defined(ARDUINO) & !defined(ETHERNET) | defined(SERIAL_DEBUG)
}

#ifdef INFO
void info(Stream& stream) {
    stream.print("Board: ");
#ifdef ARDUINO_AVR_MEGA2560
    stream.print(F("Arduino Mega2560"));
#elif defined(ARDUINO_AVR_NANO)
    stream.print(F("Arduino Nano"));
#elif defined(ARDUINO_AVR_LEONARDO)
    stream.print(F("Arduino Leonardo"));
#elif defined(ARDUINO_AVR_MINI)
    stream.print(F("Arduino Leonardo"));
#elif defined(ARDUINO_AVR_UNO)
    stream.print(F("Arduino Uno"));
#else
    stream.print(F("Unknown"));
#endif

    stream.print(F(", CPU frequency: "  EXPAND_AND_QUOTE(F_CPU)));

    stream.println();
}
#endif

//static inline bool isPrefix(const String& cmd, const char *string) {
//    return strncmp(cmd.c_str(), string, cmd.length()) == 0;
//}

static bool isPrefix(const char *string, const char* cmd) {
    return strncmp(cmd, string, strlen(string)) == 0;
}

#ifdef ARDUINO
bool isPrefix(const char* cmd, const __FlashStringHelper *pstring) {
    return strncmp_PF(cmd, reinterpret_cast<uint_farptr_t>(pstring), strlen(cmd)) == 0;
}

bool isPrefix(const __FlashStringHelper *pstring, const char* cmd) {
    return strncmp_PF(cmd, reinterpret_cast<uint_farptr_t>(pstring), strlen_PF(reinterpret_cast<uint_farptr_t>(pstring))) == 0;
}
#endif

static void readCommand(char* cmd, size_t cmdLength, StreamParser& parser) {
#if defined(COMMANDLED) & defined(LED)
    LedLcdManager::setLogicLed(commandled, LedLcdManager::on);
#endif

#ifdef ARDUINO
    //flushIn(stream);
    while (parser.getStream().available() == 0)
#endif
        LedLcdManager::checkTurnoff();


    parser.parseWord(cmd, cmdLength);
//#else
//    LedLcdManager::checkTurnoff();
//    static char str[1000];
//    parser.getLine(str, 1000);
//    if (std::cin.eof()) {
//        std::cout << "Bye!" << std::endl;
//        exit(0);
//    }
//    char *s = str;
//    while (isspace(*s))
//        s++;
//    while (isspace(s[strlen(s)-1]))
//        s[strlen(s)-1] = '\0';
//    String line(s);
//#endif

#if defined(DEBUG_CMD)
    LedLcdManager::lcdPrint(cmd, true, 0, 0);
#endif
#if defined(COMMANDLED) & defined(LED)
    LedLcdManager::setLogicLed(commandled, LedLcdManager::off);
#endif
}

static bool processCommand(const char* cmd, StreamParser& parser) {
#ifdef ETHERNET
    bool quit = false;
#endif
#ifdef ARDUINO
    Stream
#else
    std::istream
#endif
    & stream = parser.getStream();
    // Decode command
    if (cmd[0] == '\0') {
        // empty command, do nothing
#ifdef ARDUINO
        stream.println(F(okString));
#else
        std::cout << okString << std::endl;
#endif
    } else

#ifdef CAPTURE
        if (cmd[0] == 'a' || cmd[0] == 'c') {
        capture(stream);
    } else
#endif // CAPTURE

#ifdef LCD
        if (isPrefix(cmd, F("lcd"))) { //LCD
            constexpr size_t buflen = 40U;
            char rest[buflen];
            parser.getLine(rest, buflen);
        LedLcdManager::lcdPrint(rest);
        stream.println(F(okString));
        } else
#endif // LCD

#ifdef LED
        if (isPrefix(cmd, F("led"))) {
        pin_t no = static_cast<pin_t>(parser.parseAbsInt());
        char value[cmdLength];
        parser.parseWord(value, cmdLength);
        bool success = LedLcdManager::setLogicLed(no, value);
        stream.println(success ? F(okString) : F(errorString));
    } else
#endif // LED

        if (isPrefix(cmd, F("modules"))) {
#ifdef ARDUINO
        stream.println(F(modulesSupported));
#else
        std::cout << modulesSupported << std::endl;
#endif
    } else

#ifdef FREEMEM
        if (isPrefix(cmd, F("memory"))) {
        stream.println(GirsUtils::freeRam());
    } else
#endif

#ifdef INFO
        if (isPrefix(cmd, F("info"))) {
        info(stream);
    } else
#endif

#ifdef PARAMETERS
        if (cmd[0] == 'p') { // parameter
            char variableName[cmdLength];
            parser.parseWord(variableName, cmdLength);
        long value = parser.parseAbsInt();
        unsigned long *variable32 = nullptr;
        uint16_t *variable16 = nullptr;
        uint8_t *variable8 = nullptr;
#if defined(RECEIVE) || defined(CAPTURE)
           if (isPrefix(F("beg"), variableName))
            variable32 = &beginTimeout;
        else
#endif
#ifdef CAPTURE
            if (isPrefix(F("capturee"), variableName))
            variable32 = &captureEndingTimeout;
#endif
#ifdef RECEIVE
           if (isPrefix(F("receivee"), variableName))
            variable32 = &receiveEndingTimeout;
        else
#endif
#if defined(RECEIVE) & defined(IRRECEIVER_2_PIN)
            if (isPrefix(variableName, F("receiver")))
            variable8 = &receiverNo;
        else
#endif
#ifdef CAPTURE
        if (isPrefix(F("captures"), variableName)) {
        // TODO: check evenness of value
        variable16 = &captureSize;
        } else
#endif
#ifdef LED
#ifdef CONFIGURABLE_LEDS
#ifdef TRANSMITLED
            if (isPrefix(F("transmitl"), variableName))
            variable8 = &transmitled;
        else
#endif
#ifdef CAPTURELED
            if (isPrefix(F("capturel"), variableName))
            variable8 = &captureled;
        else
#endif
#ifdef RECEIVELED
            if (isPrefix(F("receivel"), variableName))
            variable8 = &receiveled;
        else
#endif
#ifdef COMMANDLED
            if (isPrefix(F("commandl"), variableName))
            variable8 = &commandled;
        else
#endif
#endif
            // TODO: handle blinkTime. This currently means either break encapsulation
            // of LedLcdManager, or write a nasty special case :-~
#endif
        {
        }

        if (variable32 != nullptr) {
            if (value != StreamParser::invalid)
                *variable32 = value;

            GirsUtils::printVariable(stream, variableName, *variable32);
        } else if (variable16 != nullptr) {
            if (value != StreamParser::invalid)
                *variable16 = static_cast<uint16_t>(value);

            GirsUtils::printVariable(stream, variableName, *variable16);
        } else if (variable8 != nullptr) {
            if (value != StreamParser::invalid)
                *variable8 = (uint8_t) value;

            GirsUtils::printVariable(stream, variableName, *variable8);
        } else
            stream.println(F("No such variable"));
    } else
#endif // PARAMETERS

#ifdef ETHERNET
        if (cmd[0] == 'q') { // quit
        quit = true;
    } else
#endif

#ifdef RECEIVE
        // TODO: option for force decoding off
        if (isPrefix(cmd, F("receive"))) { // receive
        bool status = receive(parser);
        if (!status)
            stream.println(F(errorString));
    } else
#endif // RECEIVE

#ifdef NAMED_COMMANDS
        if (cmd[0] == 'n') {
        uint16_t noSends = static_cast<uint16_t>(parser.parseAbsInt());
        String remoteName = tokenizer.getToken();
        String commandName = tokenizer.getToken();
        bool success = sendNamedCommand(stream, remoteName, commandName, noSends);
        if (success)
            stream.println(okString);
    } else

        if (isPrefix(cmd, "remote")) {
        String name = tokenizer.getToken();
        dumpRemote(stream, name);
    } else
#endif

#ifdef RESET
        if (isPrefix(cmd, "reset")) {
        reset = true;
    } else
#endif

#ifdef TRANSMIT
        if (cmd[0] == 's') { // send
        // TODO: handle unparsable data gracefully
        uint16_t noSends = static_cast<uint16_t>(parser.parseAbsInt());
        frequency_t frequency = static_cast<frequency_t>(parser.parseAbsInt());
        uint16_t introLength = static_cast<uint16_t>(parser.parseAbsInt());
        uint16_t repeatLength = static_cast<uint16_t>(parser.parseAbsInt());
        uint16_t endingLength = static_cast<uint16_t>(parser.parseAbsInt());
        microseconds_t *intro = new microseconds_t[introLength];
        parser.parseData(intro, introLength);
        microseconds_t *repeat = new microseconds_t[repeatLength];
        parser.parseData(repeat, repeatLength);
        microseconds_t *ending = new microseconds_t[endingLength];
        parser.parseData(ending, endingLength);
        IrSignal irSignal(intro, introLength, repeat, repeatLength, ending, endingLength, frequency);
        bool status = sendIrSignal(irSignal, noSends); // waits
#ifdef ARDUINO
        stream.println(status ? F(okString) : F(errorString));
#else
        std::cout << (status ? okString : errorString) << std::endl;
#endif
    } else
#endif // TRANSMIT

#ifdef PRONTO
        if (isPrefix(cmd, F("pronto"))) { // pronto hex send
        uint16_t noSends = static_cast<uint16_t>(parser.parseAbsInt());
        //String rest = tokenizer.getRest();
        IrSignal *irSignal = parser.parsePronto();
        bool status = false;
        if (irSignal != nullptr) {
            status = sendIrSignal(*irSignal, noSends); // waits
            delete irSignal;
        }
#ifdef ARDUINO
        stream.println(status ? F(okString) : F(errorString));
#else
        std::cout << (status ? okString : errorString) << std::endl;
#endif
    } else
#endif // PRONTO

#ifdef RENDERER
        if (cmd[0] == 't') { // transmit
        // TODO: handle unparseable data gracefully
        uint16_t noSends = static_cast<uint16_t>(parser.parseAbsInt());
        constexpr size_t buflen = 20U;
        char protocol[buflen];
        parser.parseWord(protocol, buflen);
        const IrSignal *irSignal = nullptr;
        if (isPrefix(protocol, F("nec1"))) {
            unsigned int D = static_cast<unsigned>(parser.parseAbsInt());
            unsigned int S = static_cast<unsigned>(parser.parseAbsInt());
            unsigned int F = static_cast<unsigned>(parser.parseAbsInt());
            irSignal = (F == StreamParser::invalid)
                    ? Nec1Renderer::newIrSignal(D, S)
                    : Nec1Renderer::newIrSignal(D, S, F);
        } else if (isPrefix(protocol, F("rc5"))) {
            unsigned int D = static_cast<unsigned>(parser.parseAbsInt());
            unsigned int F = static_cast<unsigned>(parser.parseAbsInt());
            unsigned int T = static_cast<unsigned>(parser.parseAbsInt());
            irSignal = (T == StreamParser::invalid)
                    ? Rc5Renderer::newIrSignal(D, F)
                    : Rc5Renderer::newIrSignal(D, F, T);
        } else {
#ifdef ARDUINO
            stream.print(F("no such protocol: "));
            stream.println(protocol);
#endif
        }
        bool status = false;
        if (irSignal != nullptr) {
            status = sendIrSignal(*irSignal, noSends); // waits, blinks
            delete irSignal;
        }
#ifdef ARDUINO
        stream.println(status ? F(okString) : F(errorString));
#endif
        } else
#endif // RENDERER

        if (cmd[0] == 'v') { // version
#ifdef ARDUINO
        stream.println(F(PROGNAME " " VERSION));
#else
        std::cout << PROGNAME " " VERSION << std::endl;
#endif
    } else {
#ifdef ARDUINO
        stream.println(F(errorString));
#else
        std::cout << errorString << std::endl;
#endif
    }

    //flushIn(stream);
#ifdef RESET
    if (reset)
        return false;
#endif

#ifdef ETHERNET
    if (quit)
        return false;
#endif

    return true;
}

static bool readProcessOneCommand(
#ifdef ARDUINO
        Stream
#else
        std::istream
#endif
        & stream) {
    StreamParser parser(stream);
    char cmd[cmdLength];
    //parser.parseWord(cmd, cmdLength);
    readCommand(cmd, cmdLength, parser);
#ifdef SERIAL_DEBUG
    Serial.println("Command: " + cmd);
    delay(1000);
#endif
    return processCommand(cmd, parser);
}

#if defined(ETHERNET)
bool readProcessOneTcpCommand(EthernetClient& client) {
    while (client.available() == 0) {
        LedLcdManager::checkTurnoff();
#ifdef BEACON
        Beacon::checkSend();
#endif
        if (!client.connected())
            return false;
    }
    return readProcessOneCommand(client);
}
#endif

#ifdef ARDUINO
void loop() {
    LedLcdManager::checkTurnoff();
#ifdef ETHERNET
#ifdef BEACON
    Beacon::checkSend();
#endif

    EthernetClient client = server.available();
    if (!client)
        return;
    client.setTimeout(10000);
#ifdef LCD
    LedLcdManager::lcdPrint(F("Connection from"), true, 0, 0);
    char ip[16];
    sprintf(ip, "%d.%d.%d.%d", Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);
    LedLcdManager::lcdPrint(ip, false, 0, 1);
#endif
#ifdef SERIAL_DEBUG
    Serial.print(F("Connection from "));
    Serial.println(ip);
#endif
#if defined(COMMANDLED) & defined(LED)
    LedLcdManager::setLogicLed(commandled, LedLcdManager::on);
#endif

    while (readProcessOneTcpCommand(client))
#if defined(COMMANDLED) & defined(LED)
        LedLcdManager::setLogicLed(commandled, LedLcdManager::on)
#endif
        ;
#ifdef LCD
    LedLcdManager::lcdPrint(F("Connection closed!"), true, 0, 0);
#endif
#ifdef SERIAL_DEBUG
    Serial.println(F("Connection closed!"));
#endif
    client.println(F("Bye"));

#if defined(COMMANDLED) & defined(LED)
    LedLcdManager::setLogicLed(commandled, LedLcdManager::off);
#endif

    if (client.connected())
        client.flush();
    client.stop();

#else // ! ETHERNET

    Stream& stream = Serial;
    readProcessOneCommand(stream);
#endif // ! ETHERNET

#ifdef RESET
    if (reset) {
        GirsUtils::reset();
        reset = false; // In case it does not work, do not keep trying
    }
#endif
}
#endif // ARDUINO

#ifndef ARDUINO
int main(int argc, char* argv[]) {
    setup();
//    while (true)
//        loop();
    if (argc == 1) {
        while (true) {
            constexpr size_t linemax = 1000;
            char line[linemax];
            std::cin.getline(line, linemax);
            if (line[0] == '\0' || strncmp(line, "quit", 1) == 0)
                return 0;
            std::stringstream sstream(line);
            bool success = readProcessOneCommand(sstream);
            if (!success)
                std::cerr << "**ERROR**" << std::endl;
        }
    } else {
        std::string str;
        for (int i = 1; i < argc; i++) {
            str.append(argv[i]);
            str.append(" ");
        }

        std::stringstream sstream(str);
        bool success = readProcessOneCommand(sstream);
        return success ? 0 : 1;
    }
}
#endif
