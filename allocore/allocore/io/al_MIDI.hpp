#ifndef INCLUDE_AL_IO_MIDI_HPP
#define INCLUDE_AL_IO_MIDI_HPP

#include <exception>
#include <iostream>
#include <string>
#include <vector>
#include <queue>

/**********************************************************************/
/* \class MIDI
    \brief An abstract base class for realtime MIDI input/output.

    This class implements some common functionality for the realtime
    MIDI input/output subclasses RtMidiIn and RtMidiOut.

    RtMidi WWW site: http://music.mcgill.ca/~gary/rtmidi/

    RtMidi: realtime MIDI i/o C++ classes
    Copyright (c) 2003-2010 Gary P. Scavone

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation files
    (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge,
    publish, distribute, sublicense, and/or sell copies of the Software,
    and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    Any person wishing to distribute modifications to the Software is
    requested to send the modifications to the original developer so that
    they can be incorporated into the canonical version.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/**********************************************************************/

namespace al{

class MIDIIn;

/// Convert note number to Hz value
///
/// @ingroup allocore
double noteToHz(double noteNumber);


/// Utilities for parsing MIDI bytes
///
/// @ingroup allocore
class MIDIByte{
public:

	#define BITS_(a,b,c,d,e,f,g,h)\
		(a<<7 | b<<6 | c<<5 | d<<4 | e<<3 | f<<2 | g<<1 | h)

	// Constants for checking the first message byte
	// http://www.midi.org/techspecs/midimessages.php
	static const unsigned char
		CHANNEL_MASK	= BITS_(0,0,0,0, 1,1,1,1), ///< Channel message status byte channel mask
		MESSAGE_MASK	= BITS_(1,1,1,1, 0,0,0,0), ///< Message status byte type mask

		NOTE_OFF		= BITS_(1,0,0,0, 0,0,0,0), ///< Note off channel message type
		NOTE_ON			= BITS_(1,0,0,1, 0,0,0,0), ///< Note on channel message type
		CONTROL_CHANGE	= BITS_(1,0,1,1, 0,0,0,0), ///< Control change channel message type
		 BANK_SELECT	= 0x00, ///< Bank select control number
		 MODULATION		= 0x01, ///< Modulation wheel/stick control number
		 BREATH			= 0x02, ///< Breath controller control number
		 FOOT			= 0x04, ///< Foot controller control number
		 PORTAMENTO_TIME= 0x05, ///< Portamento time control number
		 VOLUME			= 0x07, ///< Channel volume control number
		 BALANCE		= 0x08, ///< Balance control number
		 PAN			= 0x0A, ///< Pan control number
		 EXPRESSION		= 0x0B, ///< Expression controller control number
		 DAMPER_PEDAL	= 0x40, ///< Damper pedal control number
		 PORTAMENTO_ON	= 0x41, ///< Portamento on/off control number
		 SOSTENUTO_ON	= 0x42, ///< Sostenuto on/off control number
		 SOFT_PEDAL		= 0x43, ///< Soft pedal control number
		 LEGATO_ON		= 0x44, ///< Legato on/off control number

		PROGRAM_CHANGE	= BITS_(1,1,0,0, 0,0,0,0), ///< Program change channel message type
		PRESSURE_POLY	= BITS_(1,0,1,0, 0,0,0,0), ///< Polyphonic pressure (aftertouch) channel message type
		PRESSURE_CHAN	= BITS_(1,1,0,1, 0,0,0,0), ///< Channel pressure (aftertouch) channel message type
		PITCH_BEND		= BITS_(1,1,1,0, 0,0,0,0), ///< Pitch bend channel message type

		SYSTEM_MSG		= BITS_(1,1,1,1, 0,0,0,0), ///< System message type

		SYS_EX			= BITS_(1,1,1,1, 0,0,0,0), ///< System exclusive system message type
		SYS_EX_END		= BITS_(1,1,1,1, 0,1,1,1), ///< End of system exclusive system message type
		TIME_CODE		= BITS_(1,1,1,1, 0,0,0,1), ///< Time code system message type
		SONG_POSITION	= BITS_(1,1,1,1, 0,0,1,0), ///< Song position system message type
		SONG_SELECT		= BITS_(1,1,1,1, 0,0,1,1), ///< Song select system message type
		TUNE_REQUEST	= BITS_(1,1,1,1, 0,1,1,0), ///< Tune request system message type
		TIMING_CLOCK	= BITS_(1,1,1,1, 1,0,0,0), ///< Timing clock system message type
		SEQ_START		= BITS_(1,1,1,1, 1,0,1,0), ///< Start sequence system message type
		SEQ_CONTINUE	= BITS_(1,1,1,1, 1,0,1,1), ///< Continue sequence system message type
		SEQ_STOP		= BITS_(1,1,1,1, 1,1,0,0), ///< Stop sequence system message type
		ACTIVE_SENSING	= BITS_(1,1,1,1, 1,1,1,0), ///< Active sensing system message type
		RESET			= BITS_(1,1,1,1, 1,1,1,1)  ///< Reset all receivers system message type
	;
	#undef BITS_

	/// Check status byte to see if the message is a channel message
	static bool isChannelMessage(unsigned char statusByte){
		return (statusByte & MESSAGE_MASK) != SYSTEM_MSG;
	}

	/// Get string with message type from status byte
	static const char * messageTypeString(unsigned char statusByte);

	/// Get string with control type from control number
	static const char * controlNumberString(unsigned char controlNumber);

	/// Convert pitch bend message bytes into a 14-bit value in [0, 16384)
	static unsigned short convertPitchBend(unsigned char byte2, unsigned char byte3);
};


/// MIDI message
///
/// @ingroup allocore
class MIDIMessage {
public:
	unsigned char bytes[3];

	MIDIMessage(double timeStamp, unsigned port,
		unsigned char b1, unsigned char b2=0, unsigned char b3=0,
		unsigned char * data = NULL
	);

	/// Get the MIDI device port
	unsigned port() const { return mPort; }

	/// Get time stamp of message
	double timeStamp() const { return mTimeStamp; }


	/// Get the status byte
	unsigned char status() const { return bytes[0]; }

	/// Returns whether this is a channel (versus system) message
	bool isChannelMessage() const { return MIDIByte::isChannelMessage(status()); }

	/// Get the channel number (0-15)
	unsigned char channel() const { return bytes[0] & MIDIByte::CHANNEL_MASK; }

	/// Get the message type (see MIDIByte)
	unsigned char type() const { return bytes[0] & MIDIByte::MESSAGE_MASK; }


	/// Get note number (type must be NOTE_ON or NOTE_OFF)
	unsigned char noteNumber() const { return bytes[1]; }

	/// Get mapped note velocity (type must be NOTE_ON or NOTE_OFF)
	double velocity(double mul = 1./127.) const { return bytes[2]*mul; }

	/// Get mapped pitch bend amount in [-1,1] (type must be PITCH_BEND)
	double pitchBend() const {
		int v = int(MIDIByte::convertPitchBend(bytes[1], bytes[2]));
		v += 1 - bool(v); // clip interval to [1, 16383]
		return double(v - 8192) / 8191.;
	}

	/// Get controller number (type must be CONTROL_CHANGE)
	unsigned char controlNumber() const { return bytes[1]; }

	/// Get mapped controller value (type must be CONTROL_CHANGE)
	double controlValue(double mul = 1./127.) const { return bytes[2]*mul; }


	/// Get sysex message data
	unsigned char * data() const { return mData; }


	/// Print general information about message
	void print() const;

protected:
	double mTimeStamp;
	unsigned mPort;
	unsigned char * mData;
};


/// Handles receipt of MIDI messages
///
/// @ingroup allocore
class MIDIMessageHandler{
public:
	virtual ~MIDIMessageHandler(){}

	/// Called when a MIDI message is received
	virtual void onMIDIMessage(const MIDIMessage& m) = 0;

	/// Bind handler to a MIDI input
	void bindTo(MIDIIn& midiIn, unsigned port=0);

protected:
	struct Binding{
		MIDIIn * midiIn;
		MIDIMessageHandler * handler;
		unsigned port;
	};

	std::vector<Binding> mBindings;
};




/// MIDI error reporting
///
/// @ingroup allocore
class MIDIError : public std::exception
{
 public:
  //! Defined RtError types.
  enum Type {
    WARNING,           /*!< A non-critical error. */
    DEBUG_WARNING,     /*!< A non-critical error which might be useful for debugging. */
    UNSPECIFIED,       /*!< The default, unspecified error type. */
    NO_DEVICES_FOUND,  /*!< No devices found on system. */
    INVALID_DEVICE,    /*!< An invalid device ID was specified. */
    MEMORY_ERROR,      /*!< An error occured during memory allocation. */
    INVALID_PARAMETER, /*!< An invalid parameter was specified to a function. */
    INVALID_USE,       /*!< The function was called incorrectly. */
    DRIVER_ERROR,      /*!< A system driver error occured. */
    SYSTEM_ERROR,      /*!< A system error occured. */
    THREAD_ERROR       /*!< A thread error occured. */
  };

  //! The constructor.
  MIDIError( const std::string& message, Type type = MIDIError::UNSPECIFIED ) throw() : message_(message), type_(type) {}

  //! The destructor.
  virtual ~MIDIError( void ) throw() {}

  //! Prints thrown error message to stderr.
  virtual void printMessage( void ) const throw() { std::cerr << '\n' << message_ << "\n\n"; }

  //! Returns the thrown error message type.
  virtual const Type& getType(void) const throw() { return type_; }

  //! Returns the thrown error message string.
  virtual const std::string& getMessage(void) const throw() { return message_; }

  //! Returns the thrown error message as a c-style string.
  virtual const char* what( void ) const throw() { return message_.c_str(); }

 protected:
  std::string message_;
  Type type_;
};


/// @ingroup allocore
class MIDI
{
 public:

  //! Pure virtual openPort() function.
  virtual void openPort( unsigned int portNumber = 0, const std::string portName = std::string( "MIDI" ) ) = 0;

  //! Pure virtual openVirtualPort() function.
  virtual void openVirtualPort( const std::string portName = std::string( "MIDI" ) ) = 0;

  //! Pure virtual getPortCount() function.
  virtual unsigned int getPortCount() = 0;

  //! Pure virtual getPortName() function.
  virtual std::string getPortName( unsigned int portNumber = 0 ) = 0;

  //! Pure virtual closePort() function.
  virtual void closePort( void ) = 0;

 protected:

  MIDI();
  virtual ~MIDI() {};

  // A basic error reporting function for internal use in the MIDI
  // subclasses.  The behavior of this function can be modified to
  // suit specific needs.
  void error( MIDIError::Type type );

  void *apiData_;
  bool connected_;
  std::string errorString_;
};



/**********************************************************************/
/*! \class MIDIIn
    \brief A realtime MIDI input class.

    This class provides a common, platform-independent API for
    realtime MIDI input.  It allows access to a single MIDI input
    port.  Incoming MIDI messages are either saved to a queue for
    retrieval using the getMessage() function or immediately passed to
    a user-specified callback function.  Create multiple instances of
    this class to connect to more than one MIDI device at the same
    time.  With the OS-X and Linux ALSA MIDI APIs, it is also possible
    to open a virtual input port to which other MIDI software clients
    can connect.

    by Gary P. Scavone, 2003-2008.
*/
/// @ingroup allocore
/**********************************************************************/
class MIDIIn : public MIDI
{
 public:

  //! User callback function type definition.
  typedef void (*MIDICallback)( double timeStamp, std::vector<unsigned char> *message, void *userData);

  //! Default constructor that allows an optional client name.
  /*!
      An exception will be thrown if a MIDI system initialization error occurs.
  */
  MIDIIn( const std::string clientName = std::string( "MIDI Input Client") );

  //! If a MIDI connection is still open, it will be closed by the destructor.
  ~MIDIIn();

  //! Open a MIDI input connection.
  /*!
      An optional port number greater than 0 can be specified.
      Otherwise, the default or first port found is opened.
  */
  void openPort( unsigned int portNumber = 0, const std::string Portname = std::string( "MIDI Input" ) );

  //! Create a virtual input port, with optional name, to allow software connections (OS X and ALSA only).
  /*!
      This function creates a virtual MIDI input port to which other
      software applications can connect.  This type of functionality
      is currently only supported by the Macintosh OS-X and Linux ALSA
      APIs (the function does nothing for the other APIs).
  */
  void openVirtualPort( const std::string portName = std::string( "MIDI Input" ) );

  //! Set a callback function to be invoked for incoming MIDI messages.
  /*!
      The callback function will be called whenever an incoming MIDI
      message is received.  While not absolutely necessary, it is best
      to set the callback function before opening a MIDI port to avoid
      leaving some messages in the queue.
  */
  void setCallback( MIDICallback callback, void *userData = 0 );

  //! Cancel use of the current callback function (if one exists).
  /*!
      Subsequent incoming MIDI messages will be written to the queue
      and can be retrieved with the \e getMessage function.
  */
  void cancelCallback();

  //! Close an open MIDI connection (if one exists).
  void closePort( void );

  //! Return the number of available MIDI input ports.
  unsigned int getPortCount();

  //! Return a string identifier for the specified MIDI input port number.
  /*!
      An exception is thrown if an invalid port specifier is provided.
  */
  std::string getPortName( unsigned int portNumber = 0 );

  //! Set the maximum number of MIDI messages to be saved in the queue.
  /*!
      If the queue size limit is reached, incoming messages will be
      ignored.  The default limit is 1024.
  */
  void setQueueSizeLimit( unsigned int queueSize );

  //! Specify whether certain MIDI message types should be queued or ignored during input.
  /*!
      By default, MIDI timing and active sensing messages are ignored
      during message input because of their relative high data rates.
      MIDI sysex messages are ignored by default as well.  Variable
      values of "true" imply that the respective message type will be
      ignored.
  */
  void ignoreTypes( bool midiSysex = true, bool midiTime = true, bool midiSense = true );

  //! Fill the user-provided vector with the data bytes for the next available MIDI message in the input queue and return the event delta-time in seconds.
  /*!
      This function returns immediately whether a new message is
      available or not.  A valid message is indicated by a non-zero
      vector size.  An exception is thrown if an error occurs during
      message retrieval or an input connection was not previously
      established.
  */
  double getMessage( std::vector<unsigned char> *message );

  // A MIDI structure used internally by the class to store incoming
  // messages.  Each message represents one and only one MIDI message.
  struct MIDIMessage {
    std::vector<unsigned char> bytes;
    double timeStamp;

    // Default constructor.
    MIDIMessage()
      :bytes(3), timeStamp(0.0) {}
  };

  // The MIDIInData structure is used to pass private class data to
  // the MIDI input handling function or thread.
  struct MIDIInData {
    std::queue<MIDIMessage> queue;
    MIDIMessage message;
    unsigned int queueLimit;
    unsigned char ignoreFlags;
    bool doInput;
    bool firstMessage;
    void *apiData;
    bool usingCallback;
    void *userCallback;
    void *userData;
    bool continueSysex;

    // Default constructor.
    MIDIInData()
      : queueLimit(1024), ignoreFlags(7), doInput(false), firstMessage(true),
        apiData(0), usingCallback(false), userCallback(0), userData(0),
        continueSysex(false) {}
  };

 private:

  void initialize( const std::string& clientName );
  MIDIInData inputData_;

};







/**********************************************************************/
/*! \class MIDIOut
    \brief A realtime MIDI output class.

    This class provides a common, platform-independent API for MIDI
    output.  It allows one to probe available MIDI output ports, to
    connect to one such port, and to send MIDI bytes immediately over
    the connection.  Create multiple instances of this class to
    connect to more than one MIDI device at the same time.

    by Gary P. Scavone, 2003-2008.
*/

/// @ingroup allocore
/**********************************************************************/
class MIDIOut : public MIDI
{
 public:

  //! Default constructor that allows an optional client name.
  /*!
      An exception will be thrown if a MIDI system initialization error occurs.
  */
  MIDIOut( const std::string clientName = std::string( "MIDI Output Client" ) );

  //! The destructor closes any open MIDI connections.
  ~MIDIOut();

  //! Open a MIDI output connection.
  /*!
      An optional port number greater than 0 can be specified.
      Otherwise, the default or first port found is opened.  An
      exception is thrown if an error occurs while attempting to make
      the port connection.
  */
  void openPort( unsigned int portNumber = 0, const std::string portName = std::string( "MIDI Output" ) );

  //! Close an open MIDI connection (if one exists).
  void closePort();

  //! Create a virtual output port, with optional name, to allow software connections (OS X and ALSA only).
  /*!
      This function creates a virtual MIDI output port to which other
      software applications can connect.  This type of functionality
      is currently only supported by the Macintosh OS-X and Linux ALSA
      APIs (the function does nothing with the other APIs).  An
      exception is thrown if an error occurs while attempting to create
      the virtual port.
  */
  void openVirtualPort( const std::string portName = std::string( "MIDI Output" ) );

  //! Return the number of available MIDI output ports.
  unsigned int getPortCount();

  //! Return a string identifier for the specified MIDI port type and number.
  /*!
      An exception is thrown if an invalid port specifier is provided.
  */
  std::string getPortName( unsigned int portNumber = 0 );

  //! Immediately send a single message out an open MIDI output port.
  /*!
      An exception is thrown if an error occurs during output or an
      output connection was not previously established.
  */
  void sendMessage( std::vector<unsigned char> *message );

 private:

  void initialize( const std::string& clientName );
};



} // al::

#endif
