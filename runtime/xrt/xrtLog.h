#pragma once


class XRTLog {
	
public:
	static void enable() {
		getLogFlagRef()  = true;
	}
	static void xrtLog(const char * format, ...) {
	    if (getLogFlagRef()) {
	        char buffer[1024] = {};
	        va_list args;
	        va_start (args, format);
	        vsnprintf (buffer,sizeof(buffer) / sizeof(*buffer),format, args);
	        va_end (args);
	        
	        printf("[XRT]: %s\n", buffer);
	    }
	}
private:
	static bool &getLogFlagRef() {
		static bool _enabled = false;
		return _enabled;
	}
};


#define XRT_LOG_ENABLE() XRTLog::enable();
#define XRT_LOG(...) XRTLog::xrtLog(__VA_ARGS__);
