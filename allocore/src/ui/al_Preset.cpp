
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include "allocore/ui/al_Preset.hpp"
#include "allocore/io/al_File.hpp"

using namespace al;


// PresetHandler --------------------------------------------------------------

PresetHandler::PresetHandler(std::string rootDirectory, bool verbose) :
	mRootDir(rootDirectory), mVerbose(verbose), mUseCallbacks(true),
    mRunning(true), mMorphRemainingSteps(-1),
    mMorphInterval(0.05), mMorphTime("morphTime", "", 0.0, "", 0.0, 20.0), mMorphingThread(PresetHandler::morphingFunction, this)
{
	if (!File::exists(rootDirectory)) {
		if (!Dir::make(rootDirectory, true)) {
			std::cout << "Error creating directory: " << rootDirectory << std::endl;
		}
	}
	setCurrentPresetMap("default");
}

PresetHandler::~PresetHandler()
{
	stopMorph();
	mRunning = false;
	mMorphConditionVar.notify_all();
	mMorphLock.lock();
	mMorphingThread.join();
	mMorphLock.unlock();
}

PresetHandler &PresetHandler::registerParameter(Parameter &parameter)
{
	mParameters.push_back(&parameter);
	return *this;
}

void PresetHandler::setSubDirectory(std::string directory)
{
	std::string path = getRootPath();
	if (!File::exists(path + directory)) {
		if (!Dir::make(path + directory, true)) {
			std::cout << "Error creating directory: " << mRootDir << std::endl;
			return;
		}
	}
	mSubDir = directory;
}

void PresetHandler::registerPresetCallback(std::function<void (int, void *, void *)> cb,
                                           void *userData)
{
	mCallbacks.push_back(cb);
	mCallbackUdata.push_back(userData);
}


void PresetHandler::registerStoreCallback(std::function<void (int, std::string, void *)> cb, void *userData)
{
	mStoreCallbacks.push_back(cb);
	mStoreCallbackUdata.push_back(userData);
}

void PresetHandler::registerMorphTimeCallback(Parameter::ParameterChangeCallback cb,
                                              void *userData)
{
	mMorphTime.registerChangeCallback(cb, userData);
}

std::string PresetHandler::buildMapPath(std::string mapName, bool useSubDirectory)
{
	std::string currentPath = getRootPath();
	if (currentPath.back() != '/') {
		currentPath += "/";
	}
	if (useSubDirectory) {
		currentPath += mSubDir;
	}
	if (currentPath.back() != '/') {
		currentPath += "/";
	}
	if ( !(mapName.size() > 4 && mapName.substr(mapName.size() - 4) == ".txt")
	     && !(mapName.size() > 10 && mapName.substr(mapName.size() - 10) == ".presetMap")
	     && !(mapName.size() > 18 && mapName.substr(mapName.size() - 18) == ".presetMap_archive")) {
		mapName = mapName + ".presetMap";
	}

	return currentPath + mapName;
}

void PresetHandler::storePreset(std::string name)
{
	int index = -1;
	for (auto preset: mPresetsMap) {
		if (preset.second == name) {
			index = preset.first;
			break;
		}
	}
	if (index < 0) {
		for (auto preset: mPresetsMap) {
			if (index <= preset.first) {
				index = preset.first + 1;
			}
		} // FIXME this should look for the first "empty" preset.
	}
	storePreset(index, name);
}

void PresetHandler::storePreset(int index, std::string name, bool overwrite)
{
	mFileLock.lock();
	// ':' causes issues with the text format for saving, so replace
	std::replace( name.begin(), name.end(), ':', '_');

	if (name == "") {
		for (auto preset: mPresetsMap) {
			if (preset.first == index) {
				name = preset.second;
				break;
			}
		}
		if (name == "") {
			name = "default";
		}
	}
	std::map<std::string, float> values;
	for(Parameter *parameter: mParameters) {
		values[parameter->getFullAddress()] = parameter->get();
	}
	savePresetValues(values, name, overwrite);
	mPresetsMap[index] = name;
	storeCurrentPresetMap();
	mFileLock.unlock();

	if (mUseCallbacks) {
		for(int i = 0; i < mStoreCallbacks.size(); ++i) {
			if (mStoreCallbacks[i]) {
				mStoreCallbacks[i](index, name, mStoreCallbackUdata[i]);
			}
		}
	}
}

void PresetHandler::recallPreset(std::string name)
{
	{
		if (mMorphRemainingSteps.load() >= 0) {
			mMorphRemainingSteps.store(-1);
			std::lock_guard<std::mutex> lk(mTargetLock);
		}
		mTargetValues = loadPresetValues(name);
		mMorphRemainingSteps.store(1 + ceil(mMorphTime.get() / mMorphInterval));
	}
	mMorphConditionVar.notify_one();
	int index = -1;
	for (auto preset: mPresetsMap) {
		if (preset.second == name) {
			index = preset.first;
			break;
		}
	}
	mCurrentPresetName = name;
	if (mUseCallbacks) {
		for(int i = 0; i < mCallbacks.size(); ++i) {
			if (mCallbacks[i]) {
				mCallbacks[i](index, this, mCallbackUdata[i]);
			}
		}
	}
}

void PresetHandler::setInterpolatedPreset(int index1, int index2, double factor)
{
	auto presetNameIt1 = mPresetsMap.find(index1);
	auto presetNameIt2 = mPresetsMap.find(index2);
	if (presetNameIt1 != mPresetsMap.end()
	        && presetNameIt2 != mPresetsMap.end()) {

		std::map<std::string, float> values1 = loadPresetValues(presetNameIt1->second);
		std::map<std::string, float> values2 = loadPresetValues(presetNameIt2->second);
		{
			if (mMorphRemainingSteps.load() >= 0) {
				mMorphRemainingSteps.store(-1);
				std::lock_guard<std::mutex> lk(mTargetLock);
			}
			mTargetValues.clear();
			for(auto value: values1) {
				if (values2.count(value.first) > 0) {
					mTargetValues[value.first] = value.second + ((values2[value.first] - value.second)* factor);
				}
			}
		}
		mMorphConditionVar.notify_one();
	} else {
		std::cout << "Invalid indeces for preset interpolation: " << index1 << "," << index2 << std::endl;
	}
}

void PresetHandler::morphTo(ParameterStates &parameterStates, float morphTime)
{
	{
		if (mMorphRemainingSteps.load() >= 0) {
			mMorphRemainingSteps.store(-1);
			std::lock_guard<std::mutex> lk(mTargetLock);
		}
		mMorphTime.set(morphTime);
		mTargetValues = parameterStates;
		mMorphRemainingSteps.store(1 + ceil(mMorphTime.get() / mMorphInterval));
	}
	mMorphConditionVar.notify_one();
//	int index = -1;
//	for (auto preset: mPresetsMap) {
//		if (preset.second == name) {
//			index = preset.first;
//			break;
//		}
//	}
	mCurrentPresetName = "";
//	if (mUseCallbacks) {
//		for(int i = 0; i < mCallbacks.size(); ++i) {
//			if (mCallbacks[i]) {
//				mCallbacks[i](index, this, mCallbackUdata[i]);
//			}
//		}
//	}
}

std::string PresetHandler::recallPreset(int index)
{
	auto presetNameIt = mPresetsMap.find(index);
	if (presetNameIt != mPresetsMap.end()) {
		recallPreset(presetNameIt->second);
		return presetNameIt->second;
	} else {
		std::cout << "No preset index " << index << std::endl;
	}
	return "";
}

void PresetHandler::recallPresetSynchronous(std::string name)
{
	{
		if (mMorphRemainingSteps.load() >= 0) {
			mMorphRemainingSteps.store(-1);
			std::lock_guard<std::mutex> lk(mTargetLock);
		}
		mTargetValues = loadPresetValues(name);
	}
	for (Parameter *param: mParameters) {
		if (mTargetValues.find(param->getFullAddress()) != mTargetValues.end()) {
			param->set(mTargetValues[param->getFullAddress()]);
		}
	}
	int index = -1;
	for (auto preset: mPresetsMap) {
		if (preset.second == name) {
			index = preset.first;
			break;
		}
	}
	mCurrentPresetName = name;
	if (mUseCallbacks) {
		for(int i = 0; i < mCallbacks.size(); ++i) {
			if (mCallbacks[i]) {
				mCallbacks[i](index, this, mCallbackUdata[i]);
			}
		}
	}
}

std::string PresetHandler::recallPresetSynchronous(int index)
{
	auto presetNameIt = mPresetsMap.find(index);
	if (presetNameIt != mPresetsMap.end()) {
		recallPresetSynchronous(presetNameIt->second);
		return presetNameIt->second;
	} else {
		std::cout << "No preset index " << index << std::endl;
	}
	return "";
}

std::map<int, std::string> PresetHandler::availablePresets()
{
	return mPresetsMap;
}

std::string PresetHandler::getPresetName(int index)
{
	return mPresetsMap[index];
}

float PresetHandler::getMorphTime()
{
	return mMorphTime.get();
}

void PresetHandler::setMorphTime(float time)
{
	mMorphTime.set(time);
}

void PresetHandler::stopMorph()
{
	{
		if (mMorphRemainingSteps.load() >= 0) {
			mMorphRemainingSteps.store(-1);
		}
	}
	{
		std::lock_guard<std::mutex> lk(mTargetLock);
		mMorphConditionVar.notify_all();
	}
}

std::string PresetHandler::getCurrentPath()
{
	std::string relPath = getRootPath() + mSubDir;
	if (relPath.back() != '/') {
		relPath += "/";
	}
	return relPath;
}

std::string al::PresetHandler::getRootPath()
{
	std::string relPath = mRootDir;
	if (relPath.back() != '/') {
		relPath += "/";
	}
	return relPath;
}

void PresetHandler::print()
{
	std::cout << "Path: " << getCurrentPath() << std::endl;
	for (auto preset: mParameters) {
		std::cout << preset->getFullAddress() << std::endl;
	}
}

std::map<int, std::string> PresetHandler::readPresetMap(std::string mapName)
{
	std::map<int, std::string> presetsMap;
	std::string mapFullPath = buildMapPath(mapName, true);
	std::ifstream f(mapFullPath);
	if (!f.is_open()) {
		if (mVerbose) {
			std::cout << "Error while opening preset map file for reading: " << mapFullPath << std::endl;
		}
		return presetsMap;
	}
	std::string line;
	while (getline(f, line)) {
		if (line == "") {
			continue;
		}
		if (line.substr(0, 2) == "::") {
			if (mVerbose) {
				std::cout << "End preset map."<< std::endl;
			}
			break;
		}
		std::stringstream ss(line);
		std::string index, name;
		std::getline(ss, index, ':');
		std::getline(ss, name, ':');
		presetsMap[std::stoi(index)] = name;
		//			std::cout << index << ":" << name << std::endl;
	}
	if (f.bad()) {
		if (mVerbose) {
			std::cout << "Error while writing preset map file for reading: " << mFileName << std::endl;
		}
	}
	return presetsMap;
}

void PresetHandler::setCurrentPresetMap(std::string mapName, bool autoCreate)
{
	std::string mapFullPath = buildMapPath(mapName, true);
	if (autoCreate
	        && !File::exists(mapFullPath)
	        && !File::isDirectory(mapFullPath)) {
		std::cout << "No preset map. Creating default." << std::endl;
		std::vector<std::string> presets;
		Dir presetDir(getCurrentPath());
		while(presetDir.read()) {
			FileInfo info = presetDir.entry();
			if (info.type() == FileInfo::REG) {
				std::string name = info.name();
				if (name.substr(name.size()-7) == ".preset") {
					presets.push_back(info.name().substr(0, name.size()-7));
				}
			}
		}
		for(int i = 0; i < presets.size(); ++i) {
			mPresetsMap[i] = presets[i];
		}
		mCurrentMapName = mapName;
		storeCurrentPresetMap();
	} else {
		std::cout << "Set " << mapName << std::endl;
		mPresetsMap = readPresetMap(mapName);
		mCurrentMapName = mapName;
	}
}

void PresetHandler::changeParameterValue(std::string presetName,
                                         std::string parameterPath,
                                         float newValue)
{
	std::map<std::string, float> parameters = loadPresetValues(presetName);
	for (auto &parameter:parameters) {
		if (parameter.first == parameterPath) {
			parameter.second = newValue;
		}
	}
	savePresetValues(parameters, presetName, true);
}

void PresetHandler::storeCurrentPresetMap()
{
	std::string mapFullPath = buildMapPath(mCurrentMapName);
	std::ofstream f(mapFullPath);
	if (!f.is_open()) {
		if (mVerbose) {
			std::cout << "Error while opening preset map file: " << mapFullPath << std::endl;
		}
		return;
	}
	for(auto const &preset : mPresetsMap) {
		std::string line = std::to_string(preset.first) + ":" + preset.second;
		f << line << std::endl;
	}
	f << "::" << std::endl;
	if (f.bad()) {
		if (mVerbose) {
			std::cout << "Error while writing preset map file: " << mFileName << std::endl;
		}
	}
	f.close();
}

void PresetHandler::morphingFunction(al::PresetHandler *handler)
{
	handler->mMorphLock.lock();
	while(handler->mRunning) {
		std::unique_lock<std::mutex> lk(handler->mTargetLock);
		handler->mMorphConditionVar.wait(lk);
		while (std::atomic_fetch_sub(&(handler->mMorphRemainingSteps), 1) > 0) {
			for (Parameter *param: handler->mParameters) {
				float paramValue = param->get();
				if (handler->mTargetValues.find(param->getFullAddress()) != handler->mTargetValues.end()) {
					float difference =  handler->mTargetValues[param->getFullAddress()] - paramValue;
					int steps = handler->mMorphRemainingSteps.load();
					if (steps > 0) {
						difference = difference/(steps);
					}
					float newVal = paramValue + difference;
					param->set(newVal);
				}
			}
			al::wait(handler->mMorphInterval);
		}
//		// Set final values
//		for (Parameter param: mParameters) {
//			if (preset.find(param.getName()) != preset.end()) {
//				param.set(preset[param.getName()]);
//			}
//		}
	}
	handler->mMorphLock.unlock();
}

PresetHandler::ParameterStates PresetHandler::loadPresetValues(std::string name)
{
	std::map<std::string, float> preset;
	std::lock_guard<std::mutex> lock(mFileLock);
	std::string path = getCurrentPath();
	if (path.back() != '/') {
		path += "/";
	}
	std::string line;
	std::ifstream f(path + name + ".preset");
	if (!f.is_open()) {
		if (mVerbose) {
			std::cout << "Error while opening preset file: " << mFileName << std::endl;
		}
	}
	while(getline(f, line)) {
		if (line.substr(0, 2) == "::") {
			if (mVerbose) {
				std::cout << "Found preset : " << line << std::endl;
			}
			while (getline(f, line)) {
				if (line.substr(0, 2) == "::") {
					if (mVerbose) {
						std::cout << "End preset."<< std::endl;
					}
					break;
				}
				bool parameterFound = false;
				std::stringstream ss(line);
				std::string address, type, value;
				std::getline(ss, address, ' ');
				std::getline(ss, type, ' ');
				std::getline(ss, value, ' ');
				for(Parameter *param: mParameters) {
					if (mVerbose) {
						std::cout << "Checking for parameter match: " << param->getFullAddress() << std::endl;
					}
					if (param->getFullAddress() == address) {
						if (type == "f") {
							preset.insert(std::pair<std::string,float>(address,std::stof(value)));
							parameterFound = true;
							break;
						}
					}
				}
				if (!parameterFound && mVerbose) {
					std::cout << "Preset in parameter not present: " << address << std::endl;
				}
			}
		}
	}
	if (f.bad()) {
		if (mVerbose) {
			std::cout << "Error while writing preset file: " << mFileName << std::endl;
		}
	}
	f.close();
	return preset;
}

bool PresetHandler::savePresetValues(const ParameterStates &values, std::string presetName,
                                      bool overwrite)
{
	bool ok = true;
	std::string path = getCurrentPath();
	std::string fileName = path + presetName + ".preset";
	std::ifstream infile(fileName);
	int number = 0;
	while (infile.good() && !overwrite) {
		fileName = path + presetName + "_" + std::to_string(number) + ".preset";
		infile.close();
		infile.open(fileName);
		number++;
	}
	infile.close();
	std::ofstream f(fileName);
	if (!f.is_open()) {
		if (mVerbose) {
			std::cout << "Error while opening preset file: " << mFileName << std::endl;
		}
		return false;
	}
	f << "::" + presetName << std::endl;
	for(auto value: values) {
		std::string line = value.first + " f " + std::to_string(value.second);
		f << line << std::endl;
	}
	f << "::" << std::endl;
	if (f.bad()) {
		if (mVerbose) {
			std::cout << "Error while writing preset file: " << mFileName << std::endl;
		}
		ok = false;
	}
	f.close();
	return ok;
}


// PresetServer ----------------------------------------------------------------



PresetServer::PresetServer(std::string oscAddress, int oscPort) :
    mServer(nullptr), mOSCpath("/preset"),
    mAllowStore(true), mStoreMode(false),
	mNotifyPresetChange(true),
	mParameterServer(nullptr)
{
	mServer = new osc::Recv(oscPort, oscAddress.c_str(), 0.001); // Is this 1ms wait OK?
	if (mServer) {
		mServer->handler(*this);
		mServer->start();
	} else {
		std::cout << "Error starting OSC server." << std::endl;
	}
}


PresetServer::PresetServer(ParameterServer &paramServer) :
    mServer(nullptr), mOSCpath("/preset"),
    mAllowStore(true), mStoreMode(false),
    mNotifyPresetChange(true)
{
	paramServer.registerOSCListener(this);
	mParameterServer = &paramServer;
}

PresetServer::~PresetServer()
{
//	std::cout << "~PresetServer()" << std::endl;;
	if (mServer) {
		mServer->stop();
		delete mServer;
		mServer = nullptr;
	}
}

void PresetServer::onMessage(osc::Message &m)
{
	m.resetStream(); // Should be moved to the caller...
	std::cout << "PresetServer::onMessage " << std::endl;
	m.print();
	mPresetChangeLock.lock();
	mPresetChangeSenderAddr = m.senderAddress();
	if(m.addressPattern() == mOSCpath && m.typeTags() == "f"){
		float val;
		m >> val;
		if (!this->mStoreMode) {
			for (PresetHandler *handler: mPresetHandlers) {
				handler->recallPreset(static_cast<int>(val));
			}
		} else {
			for (PresetHandler *handler: mPresetHandlers) {
				handler->storePreset(static_cast<int>(val));
			}
			this->mStoreMode = false;
		}
	} else if(m.addressPattern() == mOSCpath && m.typeTags() == "s"){
		std::string val;
		m >> val;
		if (!this->mStoreMode) {
			for (PresetHandler *handler: mPresetHandlers) {
				handler->recallPreset(val);
			}
		} else {
			for (PresetHandler *handler: mPresetHandlers) {
				handler->storePreset(val);
			}
			this->mStoreMode = false;
		}
	}  else if(m.addressPattern() == mOSCpath && m.typeTags() == "i"){
		int val;
		m >> val;
		if (!this->mStoreMode) {
			for (PresetHandler *handler: mPresetHandlers) {
				handler->recallPreset(val);
			}
		} else {
			for (PresetHandler *handler: mPresetHandlers) {
				handler->storePreset(val);
			}
			this->mStoreMode = false;
		}
	} else if (m.addressPattern() == mOSCpath + "/morphTime" && m.typeTags() == "f")  {
		float val;
		m >> val;
		for (PresetHandler *handler: mPresetHandlers) {
			handler->setMorphTime(val);
		}
	} else if (m.addressPattern() == mOSCpath + "/store" && m.typeTags() == "f")  {
		float val;
		m >> val;
		if (this->mAllowStore) {
			for (PresetHandler *handler: mPresetHandlers) {
				handler->storePreset(static_cast<int>(val));
			}
		}
	} else if (m.addressPattern() == mOSCpath + "/storeMode" && m.typeTags() == "f")  {
		float val;
		m >> val;
		if (this->mAllowStore) {
			this->mStoreMode = (val != 0.0f);
		} else {
			std::cout << "Remote storing disabled" << std::endl;
		}
	} else if (m.addressPattern() == mOSCpath + "/queryState")  {
		this->mParameterServer->notifyAll();
	} else if (m.addressPattern().substr(0, mOSCpath.size() + 1) == mOSCpath + "/") {
		int index = std::stoi(m.addressPattern().substr(mOSCpath.size() + 1));
		if (m.typeTags() == "f") {
			float val;
			m >> val;
			if (static_cast<int>(val) == 1) {
				if (!this->mStoreMode) {
					for (PresetHandler *handler: mPresetHandlers) {
						handler->recallPreset(index);
					}
				} else {
					for (PresetHandler *handler: mPresetHandlers) {
						handler->storePreset(index);
					}
					this->mStoreMode = false;
				}
			}
		}
	}
	mPresetChangeLock.unlock();
	mHandlerLock.lock();
	for(osc::PacketHandler *handler: mHandlers) {
		m.resetStream();
		handler->onMessage(m);
	}
	mHandlerLock.unlock();
}

void PresetServer::print()
{
	if (mServer) {
		std::cout << "Preset server listening on: " << mServer->address() << ":" << mServer->port() << std::endl;
		std::cout << "Communicating on path: " << mOSCpath << std::endl;
		std::cout << "Registered listeners: " << std::endl;

	} else {
		std::cout << "Preset Server Connected to shared server." << std::endl;
	}
	for (auto sender:mOSCSenders) {
		std::cout << sender->address() << ":" << sender->port() << std::endl;
	}
}

void PresetServer::stopServer()
{
	if (mServer) {
		mServer->stop();
		delete mServer;
		mServer = nullptr;
	}
}

bool PresetServer::serverRunning()
{
	if (mParameterServer) {
		return mParameterServer->serverRunning();
	} else {
		return (mServer != nullptr);
	}
}

void PresetServer::setAddress(std::string address)
{
	mOSCpath = address;
}

std::string PresetServer::getAddress()
{
	return mOSCpath;
}

void PresetServer::attachPacketHandler(osc::PacketHandler *handler)
{
	mHandlerLock.lock();
	mHandlers.push_back(handler);
	mHandlerLock.unlock();
}

void PresetServer::changeCallback(int value, void *sender, void *userData)
{
	(void) sender; // remove compiler warnings
	PresetServer *server = static_cast<PresetServer *>(userData);
//	Parameter *parameter = static_cast<Parameter *>(sender);

	if (server->mNotifyPresetChange) {
		server->notifyListeners(server->mOSCpath, value);
	}
}
