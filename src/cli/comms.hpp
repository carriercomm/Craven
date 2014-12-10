#pragma once

//! Class to handle daemon--ctl comms from the ctl side.
class CommsManager
{
public:
	//! \param config The configuration for the ctl.
	//! \param output The stream to output daemon responses to.
	CommsManager(const CtlConfigure& config, std::ostream& output);

	~CommsManager();
};
