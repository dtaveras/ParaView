import sys

import SMPythonTesting
import os
import os.path
import sys
from paraview.simple import *

print 'starting'

paraview.simple._DisableFirstRenderCameraReset()

SMPythonTesting.ProcessCommandLineArguments()

fname = os.path.join(SMPythonTesting.TempDir, "parallelimagewritertest.pvti")

import paraview.servermanager
pm = paraview.servermanager.vtkProcessModule.GetProcessModule()

# if the file exists, delete it on process 0 just to be safe
processId = pm.GetGlobalController().GetLocalProcessId()
if processId == 0:
    if os.path.isfile(fname) == True:
        os.remove(fname)

Wavelet1 = Wavelet()
w = XMLPImageDataWriter()
w.FileName = fname
w.UpdatePipeline()

r = XMLPartitionedImageDataReader()
r.FileName = fname
r.UpdatePipeline()

# if we're running in symmetric mode, numcells is the local process's
# number of cells. if not, it is the global number of cells and
# only the process that reads in this script gets that information
numcells =  r.GetDataInformation().DataInformation.GetNumberOfCells()

# check which mode we're in
if pm.GetSymmetricMPIMode() == True:
    # we're in symmetric mode so we have to do a global reduce for
    # process 0 to get the global amount of cells to check
    import paraview.vtk as vtk
    da = vtk.vtkIntArray()
    da.SetNumberOfTuples(1)
    da.SetValue(0, numcells)
    da2 = vtk.vtkIntArray()
    da2.SetNumberOfTuples(1)
    pm.GetGlobalController().Reduce(da, da2, 2, 0)
    numcells = da2.GetValue(0)

if processId == 0 and numcells != 8000:
    print "ERROR: ", fname, " has ", numcells, " but should have 8000."
    sys.exit(1)

print "Test passed."
