import FWCore.ParameterSet.Config as cms
import FWCore.PythonUtilities.LumiList as LumiList
from Alignment.OfflineValidation.TkAlAllInOneTool.defaultInputFiles_cff import filesDefaultMC_NoPU

from FWCore.ParameterSet.VarParsing import VarParsing

import json
import os

##############################
# Define Process
##############################
process = cms.Process("overlap")

##############################
##Argument parsing
##############################
options = VarParsing()
options.register("config", "", VarParsing.multiplicity.singleton, VarParsing.varType.string , "AllInOne config")

options.parseArguments()

##############################
##Read in AllInOne config in JSON format
##############################
if options.config == "":
    config = {"validation": {},
              "alignment": {}}
else:
    with open(options.config, "r") as configFile:
        config = json.load(configFile)

print(config)

##############################
#Read filenames from given TXT file and define input source
##############################
readFiles = []

if "dataset" in config["validation"]:
    with open(config["validation"]["dataset"], "r") as datafiles:
        for fileName in datafiles.readlines():
            readFiles.append(fileName.replace("\n", ""))

    ##Define input source
    process.source = cms.Source("PoolSource",
                                fileNames = cms.untracked.vstring(readFiles),
                                skipEvents = cms.untracked.uint32(0)
                            )
else:
    print(">>>>>>>>>> Overlap_cfg.py: msg%-i: config not specified! Loading default MC simulation -> filesDefaultMC_NoPU!")
    process.source = cms.Source("PoolSource",
                                fileNames = filesDefaultMC_NoPU,
                                skipEvents = cms.untracked.uint32(0)
                            )

##############################
# Choose Max Events
##############################

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(config["validation"].get("maxevents", 1))
)
process.source.skipEvents=cms.untracked.uint32(0) # skipEvents is always zero

##############################
##Get good lumi section for datasets
##############################

if "goodlumi" in config["validation"]:
    if os.path.isfile(config["validation"]["goodlumi"]):
        goodLumiSecs = cms.untracked.VLuminosityBlockRange(LumiList.LumiList(filename = config["validation"]["goodlumi"]).getCMSSWString().split(','))

    else:
        print("Does not exist: {}. Continue without good lumi section file.".format())
        goodLumiSecs = cms.untracked.VLuminosityBlockRange()

else:
    goodLumiSecs = cms.untracked.VLuminosityBlockRange()

process.source.lumisToProcess = goodLumiSecs

##############################
# How to handle errors
##############################

process.options = cms.untracked.PSet(
   wantSummary = cms.untracked.bool(False),
   Rethrow = cms.untracked.vstring("ProductNotFound"), # make this exception fatal
   fileMode  =  cms.untracked.string('NOMERGE') # no ordering needed, but calls endRun/beginRun etc. at file boundaries
)

##############################
# How to report job progress
##############################

process.load("FWCore.MessageLogger.MessageLogger_cfi")
process.MessageLogger.cerr.FwkReport.reportEvery = 1000
process.MessageLogger.cout.enableStatistics = cms.untracked.bool(True)

##############################
# Track Fitting Procedure 
##############################

process.load("RecoVertex.BeamSpotProducer.BeamSpot_cff")
process.load("Configuration.Geometry.GeometryDB_cff")
process.load('Configuration.StandardSequences.Services_cff')
process.load("Configuration.StandardSequences.MagneticField_cff")


import Alignment.CommonAlignment.tools.trackselectionRefitting as trackselRefit
process.seqTrackselRefit = trackselRefit.getSequence(process, 
                                                     config["validation"].get("trackcollection", "generalTracks"),
                                                     isPVValidation=False, 
                                                     TTRHBuilder = config["validation"].get("tthrbuilder", "WithAngleAndTemplate"),
                                                     usePixelQualityFlag=config["validation"].get("usePixelQualityFlag", True),
                                                     openMassWindow = False,
                                                     cosmicsDecoMode = True,
                                                     cosmicsZeroTesla=config["validation"].get("cosmicsZeroTesla", False),
                                                     momentumConstraint = None,
                                                     cosmicTrackSplitting = False,
                                                     use_d0cut = True
                                                    )

##############################
# Define the Global Tag
##############################
process.load("Configuration.StandardSequences.FrontierConditions_GlobalTag_cff")
from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag,config["validation"].get("globaltag","auto:phase1_2024_realistic"))

##############################
# Load specific conditions 
##############################
from CalibTracker.Configuration.Common.PoolDBESSource_cfi import poolDBESSource
if "conditions" in config["alignment"]:
    for condition in config["alignment"]["conditions"]:
        setattr(process, "conditionsIn{}".format(condition), poolDBESSource.clone(
             connect = cms.string(str(config["alignment"]["conditions"][condition]["connect"])),
             toGet = cms.VPSet(
                        cms.PSet(
                                 record = cms.string(str(condition)),
                                 tag = cms.string(str(config["alignment"]["conditions"][condition]["tag"]))
                        )
                     )
            )
        )

        setattr(process, "prefer_conditionsIn{}".format(condition), cms.ESPrefer("PoolDBESSource", "conditionsIn{}".format(condition)))

##############################
# Overlap Validation Module
##############################


# Use compressions settings of TFile
# see https://root.cern.ch/root/html534/TFile.html#TFile:SetCompressionSettings
# settings = 100 * algorithm + level
# level is from 1 (small) to 9 (large compression)
# algo: 1 (ZLIB), 2 (LMZA)
# see more about compression & performance: https://root.cern.ch/root/html534/guides/users-guide/InputOutput.html#compression-and-performance
compressionSettings = 207
process.analysis = cms.EDAnalyzer("OverlapValidation",
    usePXB = cms.bool(True),
    usePXF = cms.bool(True),
    useTIB = cms.bool(True),
    useTOB = cms.bool(True),
    useTID = cms.bool(True),
    useTEC = cms.bool(True),
    compressionSettings = cms.untracked.int32(compressionSettings),
    ROUList = cms.vstring('TrackerHitsTIBLowTof',
        'TrackerHitsTIBHighTof',
        'TrackerHitsTOBLowTof',
        'TrackerHitsTOBHighTof'),
    trajectories = cms.InputTag("FinalTrackRefitter"),
    associatePixel = cms.bool(False),
    associateStrip = cms.bool(False),
    associateRecoTracks = cms.bool(False),
    #Tracks = cms.InputTag("FinalTrackRefitter"), Not sure why this throws an error but it works without is
    barrelOnly = cms.bool(False)
)


##############################
# Setup Output
##############################

process.TFileService = cms.Service("TFileService",
        fileName = cms.string("{}/Overlap.root".format(config.get("output", os.getcwd()))),
        closeFileFast = cms.untracked.bool(True),
)

#############################
# Run Sequence
#############################

process.p = cms.Path(process.seqTrackselRefit*process.analysis)