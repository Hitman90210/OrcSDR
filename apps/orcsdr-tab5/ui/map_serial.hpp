#pragma once
namespace orcsdr::maps::serial_control {
// Returns false for another command namespace. Writes complete newline-ended
// responses. Call on the UI task, using its existing authenticated session.
bool handle(const char* command,bool authenticated,void (*write)(const char*));
}
