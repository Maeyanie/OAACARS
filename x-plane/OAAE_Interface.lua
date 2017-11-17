--________________________________________________________--
--History:
--
--Version 1.0 2014-07-30 Teddii
--	release
--________________________________________________________--

--position of the interface
local XMin=50
local YMin=670

local toggle_visible=false
local winpos_moving = false

--set to "deactivate" if you want to see the interface only when hovering the mouse over it
--add_macro("Always show OAAE-Interface when on ground", "gAlwaysShowOaaeInterfaceOnGround=true", "gAlwaysShowOaaeInterfaceOnGround=false",
--			"deactivate")
--add_macro("Always show OAAE-Interface when departing", "gAlwaysShowOaaeInterfaceDeparting=true", "gAlwaysShowOaaeInterfaceDeparting=false",
--			"deactivate")

--________________________________________________________--
--________________________________________________________--
--________________________________________________________--
--________________________________________________________--
--________________________________________________________--
--________________________________________________________--
--________________________________________________________--
--________________________________________________________--
--________________________________________________________--

require "graphics"
--________________________________________________________--

DataRef("oaae_connected", 	"oaacars/connected", "writable")
DataRef("oaae_tracking",    "oaacars/tracking", "writable")
--DataRef("oaae_alive",    	"oaacars/alive", "writable")
DataRef("weight",    	"sim/flightmodel/weight/m_fixed")

oaae_connected=0 --for debug only
oaae_tracking=0  --for debug only
--oaae_alive=0     --for debug only

dataref("datRAlt", 			"sim/cockpit2/gauges/indicators/radio_altimeter_height_ft_pilot")
dataref("datGSpd", 			"sim/flightmodel/position/groundspeed")

--________________________________________________________--

local SCREEN_HEIGHT=SCREEN_HIGHT

local WIN_HEIGHT=80
local WIN_WIDTH=130
local XMax=XMin+WIN_WIDTH
local YMax=YMin+WIN_HEIGHT
local showCancelC = false

local StartButtonTimer=os.clock()

--________________________________________________________--

do_often("oaae_interface_check_often()")
do_every_draw("oaae_interface_info()")
do_on_mouse_click("oaae_interface_events()")
--________________________________________________________--

do_every_frame("oaae_interface_check()")
function oaae_interface_check()
end
--________________________________________________________--

local airborneCheckLast=0
function oaae_interface_check_often()
	if(datRAlt>25 and datGSpd>20)
	then
		if(airborneCheckLast==0) then --trigger event once(!)
			--if(oaae_tracking==0) then
			--	eventTakeOffWithoutOaaeFlightStarted();
			--end
			airborneCheckLast=1
		end
	end
	if(datRAlt<10 and datGSpd<5) then
		if(airborneCheckLast==1) then --trigger event once(!)
			if(oaae_tracking~=0) then
				toggle_visible=true
			end
			airborneCheckLast=0
		end
	end
end


local mouseHover
function oaae_interface_info()

	mouseHover=true
	-- does we have to draw anything?
	if (toggle_visible==false) then
		if MOUSE_X < XMin or MOUSE_X > XMax or MOUSE_Y < YMin or MOUSE_Y > YMax+10 then
			mouseHover=false
		end

		if(mouseHover==false) then
			if(oaae_connected==2 and oaae_tracking~=0) then
				return
			end
		end
-- FOR NOW: no connection - disable
		if(oaae_connected==0) then
			return
		end
	end

	-- moving the interface?
	if(winpos_moving==true)
	then
		min_border=25 --px
		
		big_bubble(20, 200, "Move Window x="..XMin.." y="..YMin.." ... click again to finish!")
		
		--position window to mouse (on the "M")
		XMin=MOUSE_X-5
		YMin=MOUSE_Y-WIN_HEIGHT-5

		if(XMin<=min_border) 							then XMin=min_border end
		if(YMin<=min_border) 							then YMin=min_border end
		if(XMin+WIN_WIDTH >=SCREEN_WIDTH -min_border)  	then XMin=SCREEN_WIDTH -min_border-WIN_WIDTH  end
		if(YMin+WIN_HEIGHT>=SCREEN_HEIGHT-min_border) 	then YMin=SCREEN_HEIGHT-min_border-WIN_HEIGHT end
		
		XMax=XMin+WIN_WIDTH
		YMax=YMin+WIN_HEIGHT
	end		
	
	
	-- init the graphics system
	XPLMSetGraphicsState(0,0,0,1,1,0,0)

	-- draw transparent backgroud
	if(winpos_moving) then
		graphics.set_color(1, 0, 0, 0.5)
	else
		graphics.set_color(0, 0, 0, 0.5)
	end
	
	graphics.draw_rectangle(XMin, YMin, XMax, YMax+10)

	-- draw lines around the hole block
	if(oaae_connected==0) then
		graphics.set_color(0.8, 0.8, 0.8, 0.5) 	 --grey
	elseif(oaae_connected==1) then
			graphics.set_color(1, 0, 1, 0.5) 	 --magenta
	else --connected>1
		if(oaae_tracking==0) then
			graphics.set_color(1, 0, 0, 0.5) 	 --red
		else -- tracking>0
			graphics.set_color(0, 1, 0, 0.5)	--green?
		end
	end
	graphics.set_width(2)
	graphics.draw_line(XMin, YMin, XMin, YMax+10)
	graphics.draw_line(XMin, YMax, XMax, YMax)
	graphics.draw_line(XMax, YMax+10, XMax, YMin)
	graphics.draw_line(XMax, YMin, XMin, YMin)
	graphics.draw_line(XMin, YMax+10, XMax, YMax+10)

	graphics.draw_line(XMin, 	YMin+30, XMax-1,  YMin+30) 	--hor
	--graphics.draw_line(XMin+35, YMin+1,  XMin+35, YMin+30)	--vert1
	--graphics.draw_line(XMin+40, YMin+1,  XMin+40, YMin+30)	--vert2
	--graphics.draw_line(XMin+86, YMin+1,  XMin+86, YMin+30)	--vert3
	
	--top icon row
	graphics.draw_line(XMax-12, YMax, XMax-12, YMax+10)
	graphics.draw_line(XMin+12, YMax, XMin+12, YMax+10)
	--top icon row's "M"
	graphics.draw_line(XMin+3, YMax+8, XMin+6, YMax+5)
	graphics.draw_line(XMin+9, YMax+8, XMin+6, YMax+5)
	graphics.draw_line(XMin+3, YMax+8, XMin+3, YMax+2)
	graphics.draw_line(XMin+9, YMax+8, XMin+9, YMax+2)

	--top icon row's "V"
	if(toggle_visible==true) then --if override enabled, make the V white, else stay with bg color
		graphics.set_color(0.8, 0.8, 0.8, 0.8)
	end
	graphics.draw_line(XMax-4, YMax+8, XMax-6, YMax+2)
	graphics.draw_line(XMax-8, YMax+8, XMax-6, YMax+2)

	graphics.set_color(1, 1, 1, 0.8)

	--if(oaae_connected==0) then
	--		draw_string_Helvetica_10(XMin+5, YMin+67,     "Status            : offline")
	--else
	--	if(oaae_tracking==0) then
	--		draw_string_Helvetica_10(XMin+5, YMin+67,     "Status            : on ground")
	--	else
	--		--if(oaae_airborne==0) then
	--		--	draw_string_Helvetica_10(XMin+5, YMin+67, "Status            : departing")
	--		--else
	--			draw_string_Helvetica_10(XMin+5, YMin+67, "Status            : enroute")
	--		--end
	--		--str=string.format("Flight Time      : %02i:%02i:%02i",math.floor(oaae_flighttime/3600),math.floor((oaae_flighttime%3600)/60),(oaae_flighttime%60))
	--		--draw_string_Helvetica_10(XMin+5, YMin+52, str)
	--		--str=string.format("Lease Time left: %02i:%02i:%02i",math.floor(oaae_leasetime/3600),math.floor((oaae_leasetime%3600)/60),(oaae_leasetime%60))
	--		--draw_string_Helvetica_10(XMin+5, YMin+37, str)
	--	end
	--end
	if    (oaae_connected==0) then conn="offline"
	elseif(oaae_connected==1) then conn="semi"
	elseif(oaae_connected==2) then conn="fully"
	                          else conn="unknown" end
	draw_string_Helvetica_10(XMin+5, YMin+67, "Connected: "..conn.." ("..math.ceil(oaae_connected)..")")
	--draw_string_Helvetica_10(XMin+5, YMin+52, "Tracking/Weight: "..oaae_tracking.." / "..math.ceil(weight*10)/10)
	--draw_string_Helvetica_10(XMin+5, YMin+52, "Tracking/Alive: "..oaae_tracking.." / "..math.ceil(oaae_alive))
	draw_string_Helvetica_10(XMin+5, YMin+52, "Tracking: "..oaae_tracking)
	draw_string_Helvetica_10(XMin+5, YMin+37, "Weight: "..math.ceil(weight*10)/10)
	
	--if(oaae_connected==0) then
	--		draw_string_Helvetica_10(XMin+46, YMin+11, "LOGIN")
	--else
	--	if(oaae_tracking==0) then
	--		if(StartButtonTimer<=os.clock()) then
	--			draw_string_Helvetica_10(XMin+5, YMin+17, "Start")
	--			draw_string_Helvetica_10(XMin+5, YMin+5,  "Flight")
	--		end
	--		showCancelC=false
	--	else
	--		draw_string_Helvetica_10(XMin+91, YMin+17, "Cancel")
	--		draw_string_Helvetica_10(XMin+91, YMin+5,  " ARM")
	--		if(oaae_airborne==2) then
	--			draw_string_Helvetica_10(XMin+5, YMin+17, "Finish")
	--			draw_string_Helvetica_10(XMin+5, YMin+5,  "Flight")
	--		end
	--	end
	--	if(showCancelC) then
	--		draw_string_Helvetica_10(XMin+44, YMin+17, "Cancel")
	--		draw_string_Helvetica_10(XMin+44, YMin+5,  "Confirm")
	--	end
	--end

end
--________________________________________________________--

function oaae_interface_events()
	-- we will only react once on the down event
	if MOUSE_STATUS ~= "down" then
		return
	end

	--finish change pos
	if(winpos_moving==true) then
		winpos_moving = false
		RESUME_MOUSE_CLICK = false
		return
	end
	--enable change pos
	if MOUSE_X > XMin and MOUSE_X < XMin+12 and MOUSE_Y > YMax and MOUSE_Y < YMax+10 then
			winpos_moving = true;
			if (toggle_visible==false) then
				toggle_visible=not toggle_visible
			end
			RESUME_MOUSE_CLICK = false
	end
	--toggle vis
	if MOUSE_X > XMax-12 and MOUSE_X < XMax and MOUSE_Y > YMax and MOUSE_Y < YMax+10 then
	
			toggle_visible=not toggle_visible		
			RESUME_MOUSE_CLICK = false
	end

	--start flight button
	--if MOUSE_X > XMin and MOUSE_X < XMin+36 and MOUSE_Y > YMin and MOUSE_Y < YMin+31 then
	--	if(oaae_tracking==0) then
	--		if(StartButtonTimer<=os.clock()) then
	--			command_once("fse/flight/start")
	--		else
	--			-- do nothing until timer runs out
	--		end
	--	else
	--		StartButtonTimer=os.clock()+3
	--		command_once("fse/flight/finish")
	--	end
	--	RESUME_MOUSE_CLICK = false
	--end
	--cancel flight buttons
	--if MOUSE_X > XMin+90 and MOUSE_X < XMin+120 and MOUSE_Y > YMin+5 and MOUSE_Y < YMin+25 then
	--	command_once("fse/flight/cancelArm")
	--	showCancelC=true
	--	RESUME_MOUSE_CLICK = false
	--end
	--if MOUSE_X > XMin+50 and MOUSE_X < XMin+70 and MOUSE_Y > YMin+5 and MOUSE_Y < YMin+25 then
	--	if(oaae_connected==0) then
	--		command_once("fse/server/connect")
	--	else
	--		command_once("fse/flight/cancelConfirm")
	--	end
	--	RESUME_MOUSE_CLICK = false
	--end
	
	--toogle FSE main window by clicking info text area
	--if MOUSE_X > XMin+5 and MOUSE_X < XMax-5 and MOUSE_Y > YMin+35 and MOUSE_Y < YMax-5 then
	--	command_once("fse/window/toggle")
	--	RESUME_MOUSE_CLICK = false
	--end
end
--________________________________________________________--
