#ifdef ANOMALY_DETECTOR
#define ANOMALY_DETECTOR

#include <string>
#include <vector>

#include <onnxruntime_cxx_api.h>

class Anomaly_detector{

public:
	//define
	AnomalyDetector();
	~AnomalyDetector();

	//load model
	bool loadModel(const std::string& folder);

	//reconstruction error
	float anomalyScore(const std::vector<float>& features);

	//true if anomaly score is larger than threshold value
	bool isAnomaly(const std::vector<float>& features);

private:
	//define the environment
	Ort::Env env;
	Ort::Session* session;
	Ort::SessionOptions sessionOptions;

	//defines vars to hold loaded values	
	std::vector<float> scalerMean;
	std::vector<float> scalerScale;
	float threshold;
	
	std::string inputName;
	std::string outputName;
	
	//defines consts loaded from files -scalers and threshold
	std::vector<float> standardise(const std::vector<float>& input);
	bool loadScaler(const std::string& meanFile, const std::string& scaleFile);
	float loadThreshold(const std::string& theshFile);


};

#endif
