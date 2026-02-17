import os
import copy

def Overlap(config,validationDir):
    ## List with all jobs
    jobs = []
    
    # Dictionary to track which jobs to merge
    IOVs = {}
    print(config)

    isDataMerged = {} 

    if not "single" in config["validations"]["Overlap"]:
        raise Exception("No jobs 'single' in config for Overlap")


    # Handle all of the single Overlap jobs #
    for singleName in config["validations"]["Overlap"]["single"]:
        aux_IOV = config["validations"]["Overlap"]["single"][singleName]["IOV"]
        if not isinstance(aux_IOV, list) and aux_IOV.endswith(".txt"):
            config["validations"]["Overlap"]["single"][singleName]["IOV"] = []
            with open(aux_IOV, 'r') as IOVfile:
                for line in IOVfile.readlines():
                    if len(line) != 0: config["validations"]["Overlap"]["single"][singleName]["IOV"].append(int(line))
        for IOV in config["validations"]["Overlap"]["single"][singleName]["IOV"]:
            ##Save IOV to loop later for merge jobs
            if singleName not in IOVs.keys():
                IOVs[singleName] = [] 
            if IOV not in IOVs[singleName]:
                IOVs[singleName].append(IOV)

            for alignment in config["validations"]["Overlap"]["single"][singleName]["alignments"]:
                ##Work directory for each IOV
                workDir = "{}/Overlap/{}/{}/{}/{}".format(validationDir, "single", singleName, alignment, IOV)

                ##Write local config
                local = {}
                local["output"] = "{}/{}/Overlap/{}/{}/{}/{}".format(config["LFS"], config["name"], "single", alignment, singleName, IOV)
                local["alignment"] = copy.deepcopy(config["alignments"][alignment])
                local["validation"] = copy.deepcopy(config["validations"]["Overlap"]["single"][singleName])
                local["validation"].pop("alignments")
                local["validation"]["IOV"] = IOV
                if "dataset" in local["validation"]:
                    local["validation"]["dataset"] = local["validation"]["dataset"].format(IOV)
                if "goodlumi" in local["validation"]:
                    local["validation"]["goodlumi"] = local["validation"]["goodlumi"].format(IOV)

                ##Write job info
                job = {
                    "name": "Overlap_{}_{}_{}_{}".format("single", alignment, singleName, IOV),
                    "dir": workDir,
                    "exe": "cmsRun",
                    "cms-config": "{}/src/Alignment/OfflineValidation/python/TkAlAllInOneTool/Overlap_cfg.py".format(os.environ["CMSSW_BASE"]),
                    "run-mode": "Condor",
                    "dependencies": [],
                    "config": local, 
                }

                jobs.append(job)

    if not "merge" in config["validations"]["Overlap"]:
        print("Note that you specified only single jobs. This will not produce plots")

    else:
        mergeJobs = []

        for mergeName in config["validations"]["Overlap"]["merge"]:
            singlesMC = []
            for singleName in config["validations"]["Overlap"]["merge"][mergeName]['singles']:
                if len(IOVs[singleName]) == 1 and int(IOVs[singleName][0]) == 1: singlesMC.append(singleName) 
            isMConly = (len(singlesMC) == len(config["validations"]["Overlap"]["merge"][mergeName]['singles']))
            if isMConly:
                isDataMerged[mergeName] = 0
            elif len(singlesMC) == 0:
                isDataMerged[mergeName] = 1
            else:
                isDataMerged[mergeName] = -1  

            for iname,singleName in enumerate(config["validations"]["Overlap"]["merge"][mergeName]['singles']):
                isMC = (singleName in singlesMC)
                if isMConly and iname > 0: continue #special case for MC only comparison
                elif isMConly: singlesMC.pop(singlesMC.index(singleName))
                for IOV in IOVs[singleName]:
                    workDir = "{}/Overlap/{}/{}/{}".format(validationDir, "merge", mergeName, IOV) #Different (DATA) single jobs must contain different set of IOVs

                    ##Write job info
                    local = {}

                    job = {
                        "name": "Overlap_{}_{}_{}".format("merge", mergeName, IOV),
                        "dir": workDir,
                        "exe": "OverlapPlot.py",
                        "run-mode": "Condor",
                        "dependencies": [],
                        "config": local, 
                    }
                    ##Deep copy necessary things from global config + assure plot order
                    for alignment in config["alignments"]:
                        idxIncrement = 0
                        local.setdefault("alignments", {})
                        if alignment in config["validations"]["Overlap"]["single"][singleName]["alignments"]: #Cover all DATA validations
                            local["alignments"][alignment] = copy.deepcopy(config["alignments"][alignment])
                            local["alignments"][alignment]['index'] = config["validations"]["Overlap"]["single"][singleName]["alignments"].index(alignment)
                        for singleMCname in singlesMC:
                            if alignment in config["validations"]["Overlap"]["single"][singleMCname]["alignments"]: #Add MC objects
                                local["alignments"][alignment] = copy.deepcopy(config["alignments"][alignment])
                                local["alignments"][alignment]['index']  = len(config["validations"]["Overlap"]["single"][singleName]["alignments"])
                                local["alignments"][alignment]['index'] += idxIncrement + config["validations"]["Overlap"]["single"][singleMCname]["alignments"].index(alignment)                           
                            idxIncrement += len(config["validations"]["Overlap"]["single"][singleMCname]["alignments"])   
                    local["validation"] = copy.deepcopy(config["validations"]["Overlap"]["merge"][mergeName])
                    local["validation"]["IOV"] = IOV #is it really needed here?
                    if "customrighttitle" in local["validation"].keys():
                        if "IOV" in local["validation"]["customrighttitle"]:
                            local["validation"]["customrighttitle"] = local["validation"]["customrighttitle"].replace("IOV",str(IOV)) 
                    local["output"] = "{}/{}/Overlap/{}/{}/{}/".format(config["LFS"], config["name"], "merge", mergeName, IOV)

                    ##Add global plotting options
                    if "style" in config.keys():
                        if "Overlap" in config['style'].keys():
                            if "merge" in config['style']['Overlap'].keys():
                                local["style"] = copy.deepcopy(config["style"]["Overlap"]["merge"])
                                 
 
                    ##Loop over all single jobs
                    for singleJob in jobs:
                        ##Get single job info and append to merge job if requirements fullfilled
                        _alignment, _singleName, _singleIOV = singleJob["name"].split("_")[2:]
                        if _singleName in config["validations"]["Overlap"]["merge"][mergeName]["singles"]:
                            if int(_singleIOV) == IOV or (int(_singleIOV) == 1 and _singleName in singlesMC): #matching DATA job or any MC single job 
                                local["alignments"][_alignment]["file"] = os.path.join(singleJob["config"]["output"],"Overlap.root")
                                job["dependencies"].append(singleJob["name"])
                                
                    ##Append to merge jobs  
                    mergeJobs.append(job)
    
    jobs.extend(mergeJobs)

    if "trends" in config["validations"]["Overlap"]:

        trendJobs = []

        for trendName in config["validations"]["Overlap"]["trends"]:
            #print("trendName = {}".format(trendName))
            ##Work directory for each IOV
            workDir = "{}/Overlap/{}/{}".format(validationDir, "trends", trendName)
 
            ##Write general job info
            local = {}
            job = {
                "name": "Overlap_{}_{}".format("trends", trendName),
                "dir": workDir,
                "exe": "Overlaptrends",
                "run-mode": "Condor",
                "dependencies": [],
                "config": local,
            }

            ###Loop over merge steps (merge step can contain only DATA)
            mergesDATA = []
            for mergeName in config["validations"]["Overlap"]["trends"][trendName]["merges"]:
                ##Validate merge step
                if isDataMerged[mergeName] < 0:
                    raise Exception("Trend jobs cannot process merge jobs containing both DATA and MC objects.")
                elif isDataMerged[mergeName] == 1:
                    mergesDATA.append(mergeName)
                else:
                    raise Exception("Trend jobs are not implemented for treating MC.")

            ###Loop over DATA singles included in merge steps
            trendIOVs = []
            _mergeFiles = []
            for mergeName in mergesDATA:
                for iname,singleName in enumerate(config["validations"]["Overlap"]['merge'][mergeName]['singles']): 
                    trendIOVs += [IOV for IOV in IOVs[singleName]]
                    ##Deep copy necessary things from global config + ensure plot order
                    for alignment in config["alignments"]:
                        local.setdefault("alignments", {})
                        if alignment in config["validations"]["Overlap"]["single"][singleName]["alignments"]: #Cover all DATA validations
                            local["alignments"][alignment] = copy.deepcopy(config["alignments"][alignment])
                            local["alignments"][alignment]['index'] = config["validations"]["Overlap"]["single"][singleName]["alignments"].index(alignment)
                _mergeFiles.append("{}/{}/Overlap/{}/{}/".format(config["LFS"], config["name"], "merge", mergeName)) 
            trendIOVs.sort() 
            local["validation"] = copy.deepcopy(config["validations"]["Overlap"]["trends"][trendName])
            if len(_mergeFiles) == 1:
                local["validation"]["mergeFile"] = _mergeFiles[0]
            else:
                local["validation"]["mergeFile"] = _mergeFiles #FIXME for multiple merge files in backend
            local["validation"]["IOV"] = trendIOVs
            local["output"] = "{}/{}/Overlap/{}/{}/".format(config["LFS"], config["name"], "trends", trendName)
            if "style" in config.keys() and "trends" in config["style"].keys():
                local["style"] = copy.deepcopy(config["style"])
                if "Overlap" in local["style"].keys(): local["style"].pop("Overlap") 
                if "CMSlabel" in config["style"]["trends"].keys(): local["style"]["CMSlabel"] = config["style"]["trends"]["CMSlabel"]
                if "Rlabel" in config["style"]["trends"].keys(): 
                    local["style"]["trends"].pop("Rlabel")
                    local["style"]["trends"]["TitleCanvas"] = config["style"]["trends"]["Rlabel"]
            else:
                raise Exception("You want to create 'trends' jobs, but there are no 'lines' section in the config for pixel updates!")

            #Loop over all merge jobs
            for mergeName in mergesDATA:
                for mergeJob in mergeJobs:
                    alignment, mergeJobName, mergeIOV = mergeJob["name"].split("_")[1:]
                    if mergeJobName == mergeName and int(mergeIOV) in trendIOVs:
                        job["dependencies"].append(mergeJob["name"])

            print(job)

            trendJobs.append(job)

        jobs.extend(trendJobs)
    


    return jobs
    
