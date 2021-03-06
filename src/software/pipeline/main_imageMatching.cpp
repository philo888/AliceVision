// This file is part of the AliceVision project.
// Copyright (c) 2015 AliceVision contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include <aliceVision/system/Logger.hpp>
#include <aliceVision/system/cmdline.hpp>
#include <aliceVision/voctree/Database.hpp>
#include <aliceVision/voctree/VocabularyTree.hpp>
#include <aliceVision/voctree/databaseIO.hpp>
#include <aliceVision/config.hpp>

#include <Eigen/Core>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <fstream>
#include <ostream>
#include <string>
#include <set>
#include <chrono>

static const int DIMENSION = 128;

using namespace std;
namespace po = boost::program_options;
namespace fs = boost::filesystem;


typedef aliceVision::feature::Descriptor<float, DIMENSION> DescriptorFloat;
typedef aliceVision::feature::Descriptor<unsigned char, DIMENSION> DescriptorUChar;

typedef std::size_t ImageID;

using aliceVision::IndexT;

// just a list of doc id
typedef std::vector<ImageID> ListOfImageID;

// An ordered and unique list of doc id
typedef std::set<ImageID> OrderedListOfImageID;

// For each image ID it contains the  list of matching imagess
typedef std::map<ImageID, ListOfImageID> PairList;

// For each image ID it contains the ordered list of matching images
typedef std::map<ImageID, OrderedListOfImageID> OrderedPairList;

/**
 * Function that prints a PairList
 *
 * @param os The stream on which to print
 * @param pl The pair list
 * @return the stream
 */
std::ostream& operator<<(std::ostream& os, const PairList & pl)
{
  for(PairList::const_iterator plIter = pl.begin(); plIter != pl.end(); ++plIter)
  {
    os << plIter->first;
    for(ImageID id : plIter->second)
    {
      os << " " << id;
    }
    os << "\n";
  }
  return os;
}

/**
 * Function that prints a OrderedPairList
 *
 * @param os The stream on which to print
 * @param pl The pair list
 * @return the stream
 */
std::ostream& operator<<(std::ostream& os, const OrderedPairList & pl)
{
  for(OrderedPairList::const_iterator plIter = pl.begin(); plIter != pl.end(); ++plIter)
  {
    os << plIter->first;
    for(ImageID id : plIter->second)
    {
      os << " " << id;
    }
    os << "\n";
  }
  return os;
}

enum class EImageMatchingMultiSfM
{
  A_AB,
  A_B
};


/**
 * @brief get informations about each EImageMatchingMultiSfM
 * @return String
 */
std::string EImageMatchingMultiSfM_description()
{
  return "The mode to combine matching between multiple SfM: \n"
         "* a_ab : image matching for images in input SfMData A plus between A and B\n"
         "* a_b  : image matching between input SfMData A and B\n";
}

/**
 * @brief convert an enum EImageMatchingMultiSfM to its corresponding string
 * @param modeMultiSfM
 * @return String
 */
std::string EImageMatchingMultiSfM_enumToString(EImageMatchingMultiSfM modeMultiSfM)
{
  switch(modeMultiSfM)
  {
    case EImageMatchingMultiSfM::A_AB: return "a_ab";
    case EImageMatchingMultiSfM::A_B:  return "a_b";
  }
  throw std::out_of_range("Invalid modeMultiSfM enum");
}

/**
 * @brief convert a string modeMultiSfM to its corresponding enum modeMultiSfM
 * @param String
 * @return EImageMatchingMultiSfM
 */
 EImageMatchingMultiSfM EImageMatchingMultiSfM_stringToEnum(const std::string& modeMultiSfM)
{
  std::string mode = modeMultiSfM;
  std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower); //tolower

  if(mode == "a_ab") return EImageMatchingMultiSfM::A_AB;
  if(mode == "a_b")  return EImageMatchingMultiSfM::A_B;

  throw std::out_of_range("Invalid modeMultiSfM : " + modeMultiSfM);
}

/**
 * It processes a pairlist containing all the matching images for each image ID and return
 * a similar list limited to a numMatches number of matching images and such that
 * there is no repetitions: eg if the image 1 matches with 2 in the list of image 2
 * there won't be the image 1
 *
 * @param[in] allMatches A pairlist containing all the matching images for each image of the dataset
 * @param[in] numMatches The maximum number of matching images to consider for each image
 * @param[out] matches A processed version of allMatches that consider only the first numMatches without repetitions
 */
void convertAllMatchesToPairList(const PairList &allMatches, const std::size_t numMatches, OrderedPairList &outPairList)
{
  outPairList.clear();

  PairList::const_iterator allIter = allMatches.begin();
  if(allIter == allMatches.end())
    return;

  // For the first image, just copy the numMatches first numMatches
  {
    ImageID currImageId = allIter->first;
    OrderedListOfImageID& bestMatches = outPairList[ currImageId ] = OrderedListOfImageID();
    for(std::size_t i = 0; i < std::min(allIter->second.size(), numMatches); ++i)
    {
      // avoid self-matching
      if(allIter->second[i] == currImageId)
        continue;

      bestMatches.insert(allIter->second[i]);
    }
    ++allIter;
  }

  // All other images
  for(; allIter != allMatches.end(); ++allIter)
  {
    ImageID currImageId = allIter->first;

    OrderedListOfImageID bestMatches;

    std::size_t numFound = 0;

    // otherwise check each element
    for(std::size_t i = 0; i < allIter->second.size(); ++i)
    {
      ImageID currMatchId = allIter->second[i];

      // avoid self-matching
      if(currMatchId == currImageId)
        continue;

      // if the currMatchId ID is lower than the current image ID and
      // the current image ID is not already in the list of currMatchId
      //BOOST_ASSERT( ( currMatchId < currImageId ) && ( outPairList.find( currMatchId ) != outPairList.end() ) );
      if(currMatchId < currImageId)
      {
        OrderedPairList::const_iterator currMatches = outPairList.find(currMatchId);
        if(currMatches != outPairList.end() &&
                currMatches->second.find(currImageId) == currMatches->second.end())
        {
          // then add it to the list
          bestMatches.insert(currMatchId);
          ++numFound;
        }
      }
      else if(currMatchId > currImageId)
      {
        // then add it to the list
        bestMatches.insert(currMatchId);
        ++numFound;
      }

      // if we are done stop
      if(numFound == numMatches)
        break;
    }

    // fill the output if we have matches
    if(!bestMatches.empty())
      outPairList[ currImageId ] = bestMatches;
  }
}

void generateAllMatchesInOneMap(const std::map<IndexT, std::string>& descriptorsFiles, OrderedPairList& outPairList)
{
  for(const auto& descItA: descriptorsFiles)
  {
    const IndexT imgA = descItA.first;
    OrderedListOfImageID outPerImg;

    for(const auto& descItB: descriptorsFiles)
    {
      const IndexT imgB = descItB.first;
      if(imgB > imgA)
        outPerImg.insert(imgB);
    }

    if(!outPerImg.empty())
    {
      OrderedPairList::iterator itFind = outPairList.find(imgA);

      if(itFind == outPairList.end())
        outPairList[imgA] = outPerImg;
      else
        itFind->second.insert(outPerImg.begin(), outPerImg.end());
    }
  }
}

void generateAllMatchesBetweenTwoMap(const std::map<IndexT, std::string>& descriptorsFilesA, const std::map<IndexT, std::string>& descriptorsFilesB, OrderedPairList& outPairList)
{
  for(const auto& descItA: descriptorsFilesA)
  {
    const IndexT imgA = descItA.first;
    OrderedListOfImageID outPerImg;

    for(const auto& descItB: descriptorsFilesB)
      outPerImg.insert(descItB.first);

    if(!outPerImg.empty())
    {
      OrderedPairList::iterator itFind = outPairList.find(imgA);

      if(itFind == outPairList.end())
        outPairList[imgA] = outPerImg;
      else
        itFind->second.insert(outPerImg.begin(), outPerImg.end());
    }
  }
}

int main(int argc, char** argv)
{
  using namespace aliceVision;
  namespace po = boost::program_options;

  // command-line parameters

  /// verbosity level
  std::string verboseLevel = system::EVerboseLevel_enumToString(system::Logger::getDefaultVerboseLevel());
  /// the file containing a list of features
  std::string sfmDataFilenameA;
  /// the folder containing the extracted features with their associated descriptors
  std::string featuresFolder;
  /// the filename of the voctree
  std::string treeName;
  /// the file in which to save the results
  std::string outputFile;

  // user optional parameters

  /// minimal number of images to use the vocabulary tree
  std::size_t minNbImages = 200;
  /// the file containing the list of features
  std::size_t nbMaxDescriptors = 500;
  /// the number of matches to retrieve for each image
  std::size_t numImageQuery = 50;
  /// the filename for the voctree weights
  std::string weightsName;
  /// flag for the optional weights file
  bool withWeights = false;

  // multiple SfM parameters

  /// a second file containing a list of features
  std::string sfmDataFilenameB;
  /// the multiple SfM mode
  std::string modeMultiSfMName = EImageMatchingMultiSfM_enumToString(EImageMatchingMultiSfM::A_AB);
  /// the combine SfM output
  std::string outputCombinedSfM;

  po::options_description allParams(
    "The objective of this software is to find images that are looking to the same areas of the scene. "
    "For that, we use the image retrieval techniques to find images that share content without "
    "the cost of resolving all feature matches in detail. The ambition is to simplify the image in "
    "a compact image descriptor which allows to compute the distance between all images descriptors efficiently.\n"
    "This program generates a pair list file to be passed to the aliceVision_featureMatching software. "
    "This file contains for each image the list of most similar images.\n"
    "AliceVision featureMatching");

  po::options_description requiredParams("Required parameters");
  requiredParams.add_options()
    ("input,i", po::value<std::string>(&sfmDataFilenameA)->required(),
      "SfMData file.")
    ("featuresFolder,f", po::value<std::string>(&featuresFolder)->required(),
      "Directory containing the extracted features and descriptors. By default, it is the folder containing the SfMData.")
    ("tree,t", po::value<std::string>(&treeName),
      "Input file path of the vocabulary tree. This file can be generated by createVoctree. "
      "This software is intended to be used with a generic, pre-trained vocabulary tree.")
    ("output,o", po::value<std::string>(&outputFile)->required(),
      "Filepath to the output file with the list of selected image pairs.");

  po::options_description optionalParams("Optional parameters");
  optionalParams.add_options()
    ("minNbImages", po::value<std::size_t>(&minNbImages)->default_value(minNbImages),
      "Minimal number of images to use the vocabulary tree. If we have less images than this threshold, we will compute all matching combinations.")
    ("maxDescriptors", po::value<std::size_t>(&nbMaxDescriptors)->default_value(nbMaxDescriptors),
      "Limit the number of descriptors you load per image. Zero means no limit.")
    ("nbMatches", po::value<std::size_t>(&numImageQuery)->default_value(numImageQuery),
      "The number of matches to retrieve for each image (If 0 it will "
      "retrieve all the matches).")
    ("weights,w", po::value<std::string>(&weightsName),
      "Input name for the vocabulary tree weight file, if not provided all voctree leaves will have the same weight.");

  po::options_description multiSfMParams("Multiple SfM");
  multiSfMParams.add_options()
      ("inputB", po::value<std::string>(&sfmDataFilenameB),
        "SfMData file.")
      ("modeMultiSfM", po::value<std::string>(&modeMultiSfMName)->default_value(modeMultiSfMName),
        EImageMatchingMultiSfM_description().c_str())
      ("outputCombinedSfM", po::value<std::string>(&outputCombinedSfM)->default_value(outputCombinedSfM),
        "Output file path for the combined SfMData file (if empty, don't combine).");

  po::options_description logParams("Log parameters");
  logParams.add_options()
      ("verboseLevel,v", po::value<std::string>(&verboseLevel)->default_value(verboseLevel),
        "verbosity level (fatal, error, warning, info, debug, trace).");

  allParams.add(requiredParams).add(optionalParams).add(multiSfMParams).add(logParams);

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

  // multiple SfM
  const bool useMultiSfM = !sfmDataFilenameB.empty();
  const EImageMatchingMultiSfM modeMultiSfM = EImageMatchingMultiSfM_stringToEnum(modeMultiSfMName);

  // load SfMData
  sfm::SfMData sfmDataA, sfmDataB;

  if(!sfm::Load(sfmDataA, sfmDataFilenameA, sfm::ESfMData::ALL))
  {
    ALICEVISION_LOG_ERROR("The input SfMData file '" + sfmDataFilenameA + "' cannot be read.");
    return EXIT_FAILURE;
  }

  if(useMultiSfM && !sfm::Load(sfmDataB, sfmDataFilenameB, sfm::ESfMData::ALL))
  {
    ALICEVISION_LOG_ERROR("The input SfMData file '" + sfmDataFilenameB + "' cannot be read.");
    return EXIT_FAILURE;
  }

  OrderedPairList selectedPairs;

  std::map<IndexT, std::string> descriptorsFilesA, descriptorsFilesB;

    // load descriptor filenames
  aliceVision::voctree::getListOfDescriptorFiles(sfmDataA, featuresFolder, descriptorsFilesA);

  if(useMultiSfM)
    aliceVision::voctree::getListOfDescriptorFiles(sfmDataB, featuresFolder, descriptorsFilesB);

  if(treeName.empty() && (descriptorsFilesA.size() + descriptorsFilesB.size()) > 200)
    ALICEVISION_LOG_WARNING("No vocabulary tree argument, so it will use the brute force approach which can be compute intensive for aliceVision_featureMatching.");

  if(treeName.empty() || (descriptorsFilesA.size() + descriptorsFilesB.size()) < minNbImages)
  {
    ALICEVISION_LOG_INFO("Brute force generation");
    if(modeMultiSfM == EImageMatchingMultiSfM::A_AB)
      generateAllMatchesInOneMap(descriptorsFilesA, selectedPairs);
    if(useMultiSfM)
      generateAllMatchesBetweenTwoMap(descriptorsFilesA, descriptorsFilesB, selectedPairs);
  }

  // if selectedPairs is not already computed by a brute force approach,
  // we compute it with the vocabulary tree approach.
  if(selectedPairs.empty())
  {
    // load vocabulary tree

    ALICEVISION_LOG_INFO("Loading vocabulary tree");
    auto loadVoctree_start = std::chrono::steady_clock::now();
    aliceVision::voctree::VocabularyTree<DescriptorFloat> tree(treeName);
    auto loadVoctree_elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - loadVoctree_start);
    {
      std::stringstream ss;
      ss << "tree loaded with:" << std::endl << "\t- " << tree.levels() << " levels" << std::endl;
      ss << "\t- " << tree.splits() << " branching factor" << std::endl;
      ss << "\tin " << loadVoctree_elapsed.count() << " seconds" << std::endl;
      ALICEVISION_LOG_INFO(ss.str());
    }

    // create the database
    ALICEVISION_LOG_INFO("Creating the database...");

    // add each object (document) to the database
    aliceVision::voctree::Database db(tree.words());

    if(withWeights)
    {
      ALICEVISION_LOG_INFO("Loading weights...");
      db.loadWeights(weightsName);
    }
    else
    {
      ALICEVISION_LOG_INFO("No weights specified, skipping...");
    }

    // read the descriptors and populate the database

    ALICEVISION_LOG_INFO("Reading descriptors from : ");
    ALICEVISION_LOG_INFO("\t- " << sfmDataFilenameA << " in folder " << featuresFolder);
    if(useMultiSfM)
      ALICEVISION_LOG_INFO("\t- " << sfmDataFilenameB << " in folder " << featuresFolder << ".");

    std::size_t nbFeaturesLoadedInputA = 0;
    std::size_t nbFeaturesLoadedInputB = 0;

    auto detect_start = std::chrono::steady_clock::now();
    {
      if(modeMultiSfM == EImageMatchingMultiSfM::A_AB)
        nbFeaturesLoadedInputA = aliceVision::voctree::populateDatabase<DescriptorUChar>(sfmDataA, featuresFolder, tree, db, nbMaxDescriptors);
      if(useMultiSfM)
        nbFeaturesLoadedInputB = aliceVision::voctree::populateDatabase<DescriptorUChar>(sfmDataB, featuresFolder, tree, db, nbMaxDescriptors);
    }
    auto detect_elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - detect_start);

    if((nbFeaturesLoadedInputA == 0) && (modeMultiSfM == EImageMatchingMultiSfM::A_AB))
    {
      ALICEVISION_LOG_ERROR("No descriptors loaded in '" + sfmDataFilenameA + "'");
      return EXIT_FAILURE;
    }

    if(useMultiSfM && nbFeaturesLoadedInputB == 0)
    {
      ALICEVISION_LOG_ERROR("No descriptors loaded in '" + sfmDataFilenameB + "'");
      return EXIT_FAILURE;
    }

    ALICEVISION_LOG_INFO("Read " << db.getSparseHistogramPerImage().size() << " sets of descriptors for a total of " << (nbFeaturesLoadedInputA + nbFeaturesLoadedInputB) << " features");
    ALICEVISION_LOG_INFO("Reading took " << detect_elapsed.count() << " sec.");

    if(!withWeights)
    {
      // compute and save the word weights
      ALICEVISION_LOG_INFO("Computing weights...");
      db.computeTfIdfWeights();
    }

    // query the database to get all the pair list

    if(numImageQuery == 0)
    {
      // if 0 retrieve the score for all the documents of the database
      numImageQuery = db.size();
    }

    PairList allMatches;

    ALICEVISION_LOG_INFO("Query all documents");
    detect_start = std::chrono::steady_clock::now();

    // now query each document
    #pragma omp parallel for
    for(ptrdiff_t i = 0; i < static_cast<ptrdiff_t>(descriptorsFilesA.size()); ++i)
    {
      auto itA = descriptorsFilesA.cbegin();
      std::advance(itA, i);
      const IndexT viewIdA = itA->first;
      const std::string featuresPathA = itA->second;

      aliceVision::voctree::SparseHistogram imageSH;
      if(modeMultiSfM == EImageMatchingMultiSfM::A_AB)
      {
        // sparse histogram of A is already computed in the DB
        imageSH = db.getSparseHistogramPerImage().at(viewIdA);
      }
      else
      {
        // compute the sparse histogram of each image A
        std::vector<DescriptorUChar> descriptors;
        // read the descriptors
        loadDescsFromBinFile(featuresPathA, descriptors, false, nbMaxDescriptors);
        imageSH = tree.quantizeToSparse(descriptors);
      }

      std::vector<aliceVision::voctree::DocMatch> matches;

      db.find(imageSH, numImageQuery, matches);
      //    ALICEVISION_COUT("query document " << docIt->first
      //                  << " took " << detect_elapsed.count()
      //                  << " ms and has " << matches.size()
      //                  << " matches\tBest " << matches[0].id
      //                  << " with score " << matches[0].score);

      ListOfImageID idMatches;
      idMatches.reserve(matches.size());

      for(const aliceVision::voctree::DocMatch& m : matches)
      {
        idMatches.push_back(m.id);
      }

      #pragma omp critical
      {
        allMatches[ viewIdA ] = idMatches;
      }
    }
    detect_elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - detect_start);
    ALICEVISION_LOG_INFO("Query all documents took " << detect_elapsed.count() << " sec.");

    // process pair list

    detect_start = std::chrono::steady_clock::now();

    ALICEVISION_LOG_INFO("Convert all matches to pairList");
    convertAllMatchesToPairList(allMatches, numImageQuery, selectedPairs);
    detect_elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - detect_start);
    ALICEVISION_LOG_INFO("Convert all matches to pairList took " << detect_elapsed.count() << " sec.");
  }

  // check if the output folder exists
  const auto basePath = fs::path(outputFile).parent_path();
  if(!basePath.empty() && !fs::exists(basePath))
  {
    // then create the missing folder
    if(!fs::create_directories(basePath))
    {
      ALICEVISION_LOG_ERROR("Unable to create folders: " << basePath);
      return EXIT_FAILURE;
    }
  }

  // write it to file
  std::ofstream fileout;
  fileout.open(outputFile, ofstream::out);
  fileout << selectedPairs;
  fileout.close();

  ALICEVISION_LOG_INFO("pairList exported in: " << outputFile);

  if(useMultiSfM && !outputCombinedSfM.empty())
  {
    // Combine A to B
    // Should not loose B data
    sfmDataB.combine(sfmDataA);

    if(!sfm::Save(sfmDataB, outputCombinedSfM, sfm::ESfMData::ALL))
    {
      ALICEVISION_LOG_ERROR("Unable to save combined SfM: " << outputCombinedSfM);
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
