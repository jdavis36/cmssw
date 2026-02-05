# Overlap Validation 

## General info
```
validations:
    Overlap:
        <step_type>:
            <step_name>:
                <options>
```
Overlap validation runs in sequential steps of 2 possible types:
- single (validation analysis by Overlap_cfg.py)
- merge (OverlapPlot.py script)

Step name is arbitrary string which will be used as a reference for consequent steps.
Merge job will only start if all corresponding single jobs are done.

## Single Overlap jobs
Single jobs can be specified per run (IoV as well). In case of MC, IoV is specified to arbitrary 1.  

Variable | Default value | Explanation/Options
-------- | ------------- | --------------------
IOV | None | List of IOVs/runs defined by integer value. IOV 1 is reserved for MC.
Alignments | None | List of alignments. Will create separate directory for each.
dataset | See defaultInputFiles_cff.py | Path to txt file containing list of datasets to be used. If file is missing at EOS or is corrupted - job will eventually fail (most common issue).
goodlumi | cms.untracked.VLuminosityBlockRange() | Path to json file containing lumi information about selected IoV - must contain list of runs under particular IoV with lumiblock info. Format: `IOV_Vali_{}.json`
magneticfield | true | Is magnetic field ON? Not really needed for validation...
maxevents | 1 | Maximum number of events before cmsRun terminates.
maxtracks | 1 | Maximum number of tracks per event before next event is processed.
trackcollection | "generalTracks" | Track collection to be specified here, e.g. "ALCARECOTkAlMuonIsolated" or "ALCARECOTkAlMinBias" ... 
tthrbuilder | "WithAngleAndTemplate" | Specify TTRH Builder
usePixelQualityFlag | True | Use pixel quality flag?
cosmicsZeroTesla | False | Is this validation for cosmics with zero magnetic field?
vertexcollection | "offlinePrimaryVertices" | Specify vertex collection to be used.