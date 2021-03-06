// This file is part of the AliceVision project.
// Copyright (c) 2017 AliceVision contributors.
// Copyright (c) 2013 openMVG contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include <aliceVision/system/Timer.hpp>
#include <aliceVision/system/Logger.hpp>
#include <aliceVision/system/cmdline.hpp>

#include <software/utils/sfmColorHarmonize/colorHarmonizeEngineGlobal.hpp>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <cstdlib>
#include <memory>

using namespace std;
using namespace aliceVision;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

int main( int argc, char **argv )
{
  // command-line parameters

  std::string verboseLevel = system::EVerboseLevel_enumToString(system::Logger::getDefaultVerboseLevel());
  std::string sfmDataFilename;
  std::string featuresFolder;
  std::string matchesFolder;
  std::string describerTypesName = feature::EImageDescriberType_enumToString(feature::EImageDescriberType::SIFT);
  std::string matchesGeometricModel = "f";
  std::string outputFolder ;
  int selectionMethod;
  int imgRef;

  po::options_description allParams("AliceVision sfmColorHarmonize");

  po::options_description requiredParams("Required parameters");
  requiredParams.add_options()
    ("input,i", po::value<std::string>(&sfmDataFilename)->required(),
      "SfMData file.")
    ("output,o", po::value<std::string>(&outputFolder)->required(),
      "Output path.")
    ("featuresFolder,f", po::value<std::string>(&featuresFolder)->required(),
      "Path to a folder containing the extracted features.")
    ("matchesFolder,m", po::value<std::string>(&matchesFolder)->required(),
      "Path to a folder in which computed matches are stored.")
    ("referenceImage", po::value<int>(&imgRef)->required(),
      "Reference image id.")
    ("selectionMethod", po::value<int>(&selectionMethod)->required(),
      "- 0: FullFrame\n"
      "- 1: Matched Points\n"
      "- 2: VLD Segment");

  po::options_description optionalParams("Optional parameters");
  optionalParams.add_options()
    ("describerTypes,d", po::value<std::string>(&describerTypesName)->default_value(describerTypesName),
      feature::EImageDescriberType_informations().c_str());
    ("matchesGeometricModel,g", po::value<std::string>(&matchesGeometricModel)->default_value(matchesGeometricModel),
      "Matches geometric Model :\n"
      "- f: fundamental matrix\n"
      "- e: essential matrix\n"
      "- h: homography matrix");

  po::options_description logParams("Log parameters");
  logParams.add_options()
    ("verboseLevel,v", po::value<std::string>(&verboseLevel)->default_value(verboseLevel),
      "verbosity level (fatal,  error, warning, info, debug, trace).");


  allParams.add(requiredParams).add(optionalParams).add(logParams);

  po::variables_map vm;
  try
  {
    po::store(po::parse_command_line(argc, argv, allParams), vm);

    if(vm.count("help") || (argc == 1))
    {
      ALICEVISION_COUT(allParams);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch(boost::program_options::required_option& e)
  {
    ALICEVISION_CERR("ERROR: " << e.what());
    ALICEVISION_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }
  catch(boost::program_options::error& e)
  {
    ALICEVISION_CERR("ERROR: " << e.what());
    ALICEVISION_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }

  ALICEVISION_COUT("Program called with the following parameters:");
  ALICEVISION_COUT(vm);

  // set verbose level
  system::Logger::get()->setLogLevel(verboseLevel);

  if(sfmDataFilename.empty())
  {
    ALICEVISION_LOG_ERROR("It is an invalid file input");
    return EXIT_FAILURE;
  }

  const std::vector<feature::EImageDescriberType> describerTypes = feature::EImageDescriberType_stringToEnums(describerTypesName);

  if(!fs::exists(outputFolder))
    fs::create_directory(outputFolder);

  // harmonization process

  aliceVision::system::Timer timer;

  ColorHarmonizationEngineGlobal colorHarmonizeEngine(sfmDataFilename,
    featuresFolder,
    matchesFolder,
    matchesGeometricModel,
    outputFolder,
    describerTypes,
    selectionMethod,
    imgRef);

  if(colorHarmonizeEngine.Process())
  {
    ALICEVISION_LOG_INFO(" ColorHarmonization took: " << timer.elapsed() << " s");
    return EXIT_SUCCESS;
  }
  else
  {
    ALICEVISION_LOG_ERROR("Something goes wrong in the process");
  }
  return EXIT_FAILURE;
}
