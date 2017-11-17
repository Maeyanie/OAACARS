from XPStandardWidgets import *
from XPLMProcessing import *
from XPLMDataAccess import *
from XPLMNavigation import *
from XPLMUtilities import *
from XPLMGraphics import * 
from XPWidgetDefs import *
from XPLMDisplay import *
from XPLMMenus import *
from XPWidgets import *
from XPLMPlanes import *

##########################################################################################################################
## the main plugin interface class
class PythonInterface:
	def XPluginStart(self):
		self.Name = "OAACARS Datarefs"
		self.Sig =  "OAACARS.Python.Datarefs"
		self.Desc = "OAACARS Datarefs"
		self.VERSION="0.11"

		self.connected=0
		self.tracking=0
		self.alive=0

		#register CustomDataRef
		self.tempCB0      = self.CallbackDatarefConnected_Rd
		self.tempCB1      = self.CallbackDatarefConnected_Wr
		self.drConnected  = XPLMRegisterDataAccessor(self, "oaacars/connected",  xplmType_Float, 1, 
			None, None, 
			self.tempCB0, self.tempCB1, 
			None, None, 
			None, None, 
			None, None, 
			None, None, 0, 0)

		self.tempCB2      = self.CallbackDatarefTracking_Rd
		self.tempCB3      = self.CallbackDatarefTracking_Wr
		self.drTacking    = XPLMRegisterDataAccessor(self, "oaacars/tracking",   xplmType_Float, 1,
			None, None, 
			self.tempCB2, self.tempCB3, 
			None, None,
			None, None, 
			None, None, 
			None, None, 0, 0)
		
		self.tempCB4      = self.CallbackDatarefAlive_Rd
		self.tempCB5      = self.CallbackDatarefAlive_Wr
		self.drAlive      = XPLMRegisterDataAccessor(self, "oaacars/alive",   xplmType_Float, 1,
			None, None, 
			self.tempCB4, self.tempCB5,
			None, None,
			None, None, 
			None, None, 
			None, None, 0, 0)
		
		return self.Name, self.Sig, self.Desc

	def XPluginStop(self):
		XPLMUnregisterDataAccessor(self, self.drConnected)
		XPLMUnregisterDataAccessor(self, self.drTacking)
		XPLMUnregisterDataAccessor(self, self.drAlive)
		return 1

	def XPluginEnable(self):
		return 1

	def XPluginDisable(self):
		return 1

	def XPluginReceiveMessage(self, inFromWho, inMessage, inParam):
		return 0

	#############################################################
	## Callback handler for reading custom datarefs
	def CallbackDatarefConnected_Rd(self, inRefcon):
		return self.connected
	def CallbackDatarefTracking_Rd(self, inRefcon):
		return self.tracking
	def CallbackDatarefAlive_Rd(self, inRefcon):
		return self.alive

	def CallbackDatarefConnected_Wr(self, inRefcon, inValue):
		self.connected=inValue
		pass
	def CallbackDatarefTracking_Wr(self, inRefcon, inValue):
		self.tracking=inValue
		pass
	def CallbackDatarefAlive_Wr(self, inRefcon, inValue):
		self.alive=inValue
		pass

	#The End
