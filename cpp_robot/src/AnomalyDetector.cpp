//#include "AnomalyDetector.h"

#include <torch/torch.h>
#include <torch/script.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>
#include <vector>
#include <string>
#include <filesystem>

struct AnomalyResult{
  float score;
  bool isAnomaly;
};

class AnomalyDetector {

public:

  AnomalyDetector(){
    threshold = 0.0f;
    modelLoaded = false;
    featureCount = 0;
  }
  
  bool loadModel(const std::string& folderPath){

        try {
                //extract filepaths
                std::string modelPath = folderPath + "model.pt";
               // std::string modelPath = ament_index_cpp::get_package_share_directory("cpp_robot") + folderPath + "/model.pt";
                std::string threshPath = folderPath + "/threshold.txt";
                
                //std::string modelPath = "/home/justine/ros2_ws2/src/cpp_robot/models/Model1/model1.pt";
                std::filesystem::path modelFolder(modelPath);
                
                
                //load model
                model = torch::jit::load(modelFolder);
                
                model.eval();
                
                //read threshold from file
                std::ifstream file(threshPath);
                
                
            
                if (!file.is_open()){
                    std::cerr << "Missing threshold\n";
                    return false;
                }
                
                file >> threshold;
                file.close();
                
                if(!loadScaler(folderPath)){
                  return false;
                }
                
                modelLoaded = true;
                std::cout << "Loaded " << folderPath << "\n";
                //std::cout << "Sclars" << mean.size() << "\n";
                return true;
        }
        catch(const c10::Error& error){
                std::cerr << "Libtorch error " << error.what() << "\n";
                return false;
        }

  }

  
//features must be in same order as training
  float anomalyScore(const std::vector<float>& features){
	std::vector<float> scaled_features = standardise(features);
	//ensures compatible typeing
	torch::Tensor input = createTensor(scaled_features);
	
	//just inference
	torch::NoGradGuard noGrad;
	std::vector<torch::jit::IValue> inputs;
	
	inputs.push_back(input);
	
	//reconstruct
	torch::Tensor reconstruct = model.forward(inputs).toTensor();
	
	//mse, (diff of input - reconstruct)^2/inputs size
	torch::Tensor error = torch::mean(torch::pow(input - reconstruct, 2));
	
	return error.item<float>();

  }

//Features must be same layout as for training
  bool isAnomaly(const std::vector<float>& features){
	//get anomaly score and compare to threshold
	float score = anomalyScore(features);
	return score > threshold;
  }

  float getThreshold(){
        return threshold;
  }
  
private:
  torch::jit::script::Module model;
  float threshold;
  bool modelLoaded;
  std::vector<float> mean;
  std::vector<float> scale;
  int featureCount;
  
  bool loadScaler(const std::string& folderPath){
        std::string scalePath = folderPath + "/scaler_scale.csv";
        std::string meanPath = folderPath + "/scaler_mean.csv";
        std::string featurePath = folderPath + "/features.txt";
        
        //read scalers
        std::ifstream meanFile(meanPath);
        mean.clear();
                
        std::ifstream scaleFile(scalePath);
        scale.clear();
        //err handling        
        if (!meanFile.is_open() || !scaleFile.is_open()){
              std::cerr << "Missing Scaler\n";
              return false;
        }
        
        //bc must be same size
        std::ifstream fFile(featurePath);
        featureCount = 0;
        std::string line;
        //length of file
        while (!fFile.eof()){
            getline(fFile, line);
            featureCount++;
        
        }
      
        //fFile.size();
        
        //mean.resize(featureCount);
        //scale.resize(featureCount);
        
        float value;
        
        //assumes all scaled --- mismatch
          //for values in mean
        //for(int i=0; i<scaledCount;i++){
         // meanFile >> mean[i];
         // scaleFile >> scale[i];
        //}
        scale.clear();
        mean.clear();
        while(meanFile >> value){
          mean.push_back(value);
        }
        
        while(scaleFile >> value){
          scale.push_back(value);
        }
        
        std::cout << featureCount << " - Feature size \n";
        std::cout << mean.size() << " - Mean size\n";
        std::cout << scale.size() << " - Scale size\n";
        
        //cleanup
        meanFile.close();
        scaleFile.close();
        fFile.close();
	return true;

  }
  
  std::vector<float> standardise(const std::vector<float>& features){
       // if (features.size() != mean.size()){
          //throw std::runtime_error("Features do not match scaler");
        //}
        
	std::vector<float> result;

		
	//normalised features 
	for (size_t i=0; i<mean.size(); i++){
	      float scaled = (features[i] - mean[i]) / scale[i];
	      result.push_back(scaled);
	      std::cout << scaled << " - scaled \n";
	}
	std::cout << result.size() << " - Result size -- only scaled \n";
	std::cout << features.size() << " --- features -- only scaled \n ";
	//non normalised ie bool
	for (size_t i=mean.size();i<features.size();i++){
	      result.push_back(features[i]);
	}
	std::cout << result.size() << " - Result size \n";
         
	return result;
  }
  
  torch::Tensor createTensor(const std::vector<float>& features){
        torch::Tensor tensor = 
              torch::from_blob(
                    (float*)features.data(),
                    { 1, (long)features.size()},
                    torch::kFloat32
              ).clone();
        return tensor;
  }

};











