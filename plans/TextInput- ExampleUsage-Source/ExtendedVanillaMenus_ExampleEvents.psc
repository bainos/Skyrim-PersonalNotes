Scriptname ExtendedVanillaMenus_ExampleEvents extends quest 
;in this example, we'll use mod events with the WaitForResult param = false, so opening the menus won't pause the current script function.
;Note that you can still use events if the WaitForResult param is true.
;Events make it so you can run script commands while the menu is open and add for example sound effects, as demonstrated with the TweenMenuTest().
;This script is fully functioning on it's own, so you can use it as a preview. Attach it to a new quest, then press Left Alt in game to see how the menus work.

Int Property MyColor = 1541020 Auto Hidden

Float Property MyFloat = 1.0 Auto Hidden ;for using only a single slider
String Property MyFloatSliderParams Auto Hidden ;single slider param string.

Float[] MySliderValues ;for multiple sliders in menu
String[] MySliders ;multiple slider params. Indexes should match with MySliderValues

String[] RandomWords ;for testing messageBox menu.

String InputString = ""

Event OnInit() 
    registerForModEvent("EVM_ButtonMenuDebug", "OnEVM_ButtonMenuDebug")
    
    ;save slider params to string for use in slider menu. Use a YourModName_ prefix for SliderID's, as that's what's used to send mod events. Also all SliderID's should be unique.
    MyFloatSliderParams = ExtendedVanillaMenus.SliderParamsToString(SliderID = "MyMod_SingleSlider0", SliderText = "", InfoText = "My Slider Info", Minimum = 0, Maximum = 10, Interval = 0.05, DecimalPlaces = 2)    
    
    ;this is the same as above. Note that SliderText isn't needed as there's only 1 slider. SliderText is what appears above individual sliders.
    MyFloatSliderParams = ExtendedVanillaMenus.SliderParamsToString("MyMod_SingleSlider0", "", "My Slider Info", 0, 10, 0.05, 2) 
    
    MySliderValues = New Float[3] ;the initial values for MySliders
    MySliderValues[0] = 0
    MySliderValues[1] = 0.5
    MySliderValues[2] = 1.01 
    
    MySliders = new String[3] ;slider params, lines up with MySliderValues array.
    MySliders[0] = ExtendedVanillaMenus.SliderParamsToString("MyMod_SliderA", "Slider A", "Slider A Info", 0, 100, 1, 0)
    MySliders[1] = ExtendedVanillaMenus.SliderParamsToString("MyMod_SliderB", "Slider B", "Slider B Info", 0, 10, 0.1, 1)
    MySliders[2] = ExtendedVanillaMenus.SliderParamsToString("MyMod_SliderC", "Slider C", "Slider C Info", 1, 2, 0.01, 2)
    
	debug.Notification("ExtendedVanillaMenus_ExampleEvents installed")
    registerForKey(56) ;left alt
EndEvent 

Event OnEVM_ButtonMenuDebug(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    if a_numArg == 0 
        debug.trace(a_strArg)
    elseif a_numArg == 1 
        debug.notification(a_strArg)
    elseif a_numArg == 2
        debug.MessageBox(a_strArg)
    Endif
EndEvent

Event OnKeyDown(Int keyCode) 
    if !ui.IsTextInputEnabled() && !Utility.IsInMenuMode()
        MyMessageMenu()
    Endif
EndEvent

function MyMessageMenu()
    String akMessage = "Choose a menu to open"
    
     String[] MyButtons = New String[9] 
    
    MyButtons[0] = "Exit" 
    MyButtons[1] = "Color Picker"
    MyButtons[2] = "Single Slider"
    MyButtons[3] = "Multiple Sliders"
    MyButtons[4] = "List Menu"
    MyButtons[5] = "Message Box" 
    MyButtons[6] = "Default Text Input" 
    MyButtons[7] = "Custom Text Input"   
    MyButtons[8] = "Tween Menu"   
    
    ;register for mod event first
    RegisterForModEvent("EVM_ButtonMenuClosed", "OnEVM_ButtonMenuClosed")
    
    ;open MessageBox Button menu, don't wait for result.
    ;if CancelIsFirstButton == true, snaps to first button when pressing Tab or Escape. Otherwise snaps to last button.
    ExtendedVanillaMenus.MessageBox(sMessage = akMessage, Buttons = MyButtons, CancelIsFirstButton = true, WaitForResult = false)
    
    ;ExtendedVanillaMenus.MessageBox(akMessage, Buttons, false, false) ;this is the same as above
EndFunction

Event OnEVM_ButtonMenuClosed(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    UnRegisterForModEvent("EVM_ButtonMenuClosed") ;unregister event first so it isn't recieved from other mods opening the MessageBox menu.
    int Button = a_numArg as int
    
	If Button == 1 
        SetMyColor() 
    Elseif Button == 2
        SetMyFloat()
    Elseif Button == 3
        SetMySliderValues() 
    Elseif Button == 4
        TestListMenu()
    Elseif Button == 5
        TestMyMessageMenu()
    Elseif Button == 6
        DefaultInputMenu()
    Elseif Button == 7
        CustomInputMenu()
    Elseif Button == 8
		TweenMenuTest()
	Endif
EndEvent

;color picker==========================================================================================================
Function SetMyColor() 
    RegisterForModEvent("EVM_ColorPickerChanged", "OnEVM_ColorPickerChanged")
    RegisterForModEvent("EVM_ColorPickerClosed", "OnEVM_ColorPickerClosed")
    
    ;open color picker menu, and don't wait for result.
    ExtendedVanillaMenus.ColorPicker(MyColor, "My Color", "My Color Info", WaitForResult = false)
EndFunction
    
Event OnEVM_ColorPickerChanged(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    ;this event is sent from flash whenever the color is changed, so you can update something in real time.
    int akColor = a_numArg as int 
    ;debug.Notification("Color = " + akColor)
EndEvent

Event OnEVM_ColorPickerClosed(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    ;unregister for mod events first so they aren't recieved from other scripts opening the color picker.
    UnRegisterForModEvent("EVM_ColorPickerChanged")
    UnRegisterForModEvent("EVM_ColorPickerClosed")
    
    ;<font color='#ff0000'>
    
    If a_strArg == "Accepted" 
        MyColor = a_numArg as int 
        string MyColorHex = DbMiscFunctions.ConvertintToHex(MyColor)
        ExtendedVanillaMenus.MessageBox("<font color='#" + MyColorHex + "'>" + "MyColor = " + MyColor)
    elseif a_strArg == "Canceled"
        ;user canceled 
    Endif
EndEvent

;single slider===========================================================================================================
Function SetMyFloat()
    ;open single Slider Menu and save result to MyFloat. 
    ;Register for mod events before opening the menu.
    
    registerForModEvent("EVM_SliderChanged_" + "MyMod_SingleSlider0", "OnMyMod_SingleSlider0Changed")
    registerForModEvent("EVM_SliderMenuClosed", "OnEVM_SliderMenuClosed")
    ExtendedVanillaMenus.SliderMenu(MyFloatSliderParams, MyFloat, "$My Slider", WaitForResult = False)
EndFunction

Event OnMyMod_SingleSlider0Changed(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    ;this event is sent from flash whenever the MyMod_SingleSlider0 is changed, so you can update something in real time.
    ;debug.notification("slider value = " + a_numArg)
EndEvent

Event OnEVM_SliderMenuClosed(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    ;this event is sent from flash when the slider menu is closed
    UnRegisterForModEvent("EVM_SliderChanged_" + "MyMod_SingleSlider0")
    UnRegisterForModEvent("EVM_SliderMenuClosed")
    
    If a_numArg == 1
        ;user accepted
        MyFloat = a_strArg as float
        Debug.MessageBox("MyFloat = " + MyFloat)
    elseif a_numArg == 0
        ;user canceled 
    Endif
EndEvent 
;============================================================================================================================ 


;Multiple Sliders ==============================================================================================================================================
Function SetMySliderValues()
    ;open Slider Menu with multiple sliders. Accept Button Text = $Alright. Cancel Button Text = $No Way.
    ;Register for mod events before opening the menu.
    ;Slider change events are EVM_SliderChanged_ + SliderID
    
    registerForModEvent("EVM_SliderChanged_" + "MyMod_SliderA", "OnMyMod_SliderAChanged") ;event recieved when MyMod_SliderA changes while in the Slider Menu
    registerForModEvent("EVM_SliderChanged_" + "MyMod_SliderB", "OnMyMod_SliderBChanged")
    registerForModEvent("EVM_SliderChanged_" + "MyMod_SliderC", "OnMyMod_SliderCChanged")
    
    ;event recieved when Slider Menu is closed. a_strArg == slider values seperated by ||. If accepted, a_numArg == 1. If Cancelled a_numArg == 0.
    registerForModEvent("EVM_SliderMenuClosed", "OnEVM_MultSliderMenuClosed")
    
    ExtendedVanillaMenus.SliderMenuMult(SliderParams = MySliders, InitialValues = MySliderValues, TitleText = "My Sliders", AcceptText = "Alright", CancelText = "No Way", WaitForResult = False)
    ;ExtendedVanillaMenus.SliderMenuMult(MySliders, MySliderValues, "My Sliders", "Alright", "No Way", WaitForResult = False) ;this is the same as above.
EndFunction

Event OnMyMod_SliderAChanged(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    ;this event is sent from flash whenever the MyMod_SliderA is changed, so you can update something in real time.
    ;debug.notification("MyMod_SliderA = " + a_numArg)
EndEvent

Event OnMyMod_SliderBChanged(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    ;this event is sent from flash whenever the MyMod_SliderB is changed, so you can update something in real time.
    ;debug.notification("MyMod_SliderB = " + a_numArg)
EndEvent

Event OnMyMod_SliderCChanged(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    ;this event is sent from flash whenever the MyMod_SliderC is changed, so you can update something in real time.
    ;debug.notification("MyMod_SliderC = " + a_numArg)
EndEvent

Event OnEVM_MultSliderMenuClosed(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    UnRegisterForModEvent("EVM_SliderMenuClosed") ;unregister events first so they aren't recieved from other mods opening the menu
    UnregisterForModEvent("EVM_SliderChanged_" + "MyMod_SliderA")
    UnregisterForModEvent("EVM_SliderChanged_" + "MyMod_SliderB")
    UnregisterForModEvent("EVM_SliderChanged_" + "MyMod_SliderC")
    
    if a_numArg == 1 ;user accepted 
        Float[] NewValues = DbMiscFunctions.SplitAsFloat(a_strArg) ;split string result into float array
        if NewValues.length == MySliderValues.length ;if NewValues array is the same length as our original float array, assume everything is a ok
            MySliderValues = NewValues 
            Debug.MessageBox("MySliderValues = " + MySliderValues)
        Endif
    elseif a_numArg == 0
        ;user canceled 
    Endif
EndEvent

;list menu ==============================================================================================================================================
Function TestListMenu()
	String ipsum = GetLoremipsumNoPunctuation()
	String[] ipsumWords = StringUtil.Split(ipsum, " ")
	int ipsumSize = ipsumWords.length - 1
	
    int listLength = extendedVanillaMenus.textInput("Set List Length") as int
	String[] Options = Utility.CreateStringArray(ListLength)
	String[] OptionInfos = Utility.CreateStringArray(ListLength)
	int i = 0
	While i < ListLength
		string option = ipsumWords[Utility.RandomInt(0, ipsumSize)]
		int numofWords = utility.randomint(1, 7) 
		while numofWords > 0
			option += (" " + ipsumWords[Utility.RandomInt(0, ipsumSize)])
			numofWords -= 1
		EndWhile
			
		Options[i] = i + ": " + option
		OptionInfos[i] = (Options[i] + " Info")

        int enabled = utility.randomInt(0, 1)
        if enabled == 0
            Options[i] = (Options[i] + "<enabled=0>") ;disable button, won't show in menu.
        endif

		i += 1
	EndWhile
	
	RegisterForModEvent("EVM_ListMenuClosed", "OnEVM_ListMenuClosed")
    ExtendedVanillaMenus.ListMenu(Options, OptionInfos, "My Options", "Okay", "No Way", false)
EndFunction

Event OnEVM_ListMenuClosed(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    UnRegisterForModEvent("EVM_ListMenuClosed") 
    int Button = a_numArg as int
    Debug.MessageBox("Button = " + Button + " option = " + a_strArg)
EndEvent

;testing messageBox button menu =============================================================================================================================================================
function TestMyMessageMenu()
    String akMessage = GetLoremipsumNoPunctuation()
    
    int i = 0 
    While i < 30 
        akMessage += "\nTesting " + i ;expand message down to test vertical scroll
        i += 1
    EndWhile
    
	String ipsum = GetLoremipsumNoPunctuation()
	String[] ipsumWords = StringUtil.Split(ipsum, " ")
	int ipsumSize = ipsumWords.length - 1
	
    int listLength = extendedVanillaMenus.textInput("Set number of buttons.") as int
	String[] Buttons = Utility.CreateStringArray(ListLength)
	i = 0
	While i < ListLength
		string buttonText = ipsumWords[Utility.RandomInt(0, ipsumSize)]
		int numofWords = utility.randomint(1, 3) 
		while numofWords > 0
			buttonText += (" " + ipsumWords[Utility.RandomInt(0, ipsumSize)])
			numofWords -= 1
		EndWhile
			
		Buttons[i] = i + ": " + buttonText

        int enabled = utility.randomInt(0, 1)
        if enabled == 0
            Buttons[i] = (Buttons[i] + "<enabled=0>") ;disable button, won't show in menu.
        endif

		i += 1
	EndWhile

    RegisterForModEvent("EVM_ButtonMenuClosed", "OnEVM_TestMyMessageMenuClosed")
    ExtendedVanillaMenus.MessageBox(akMessage, Buttons, false, waitforResult = false)
EndFunction

Event OnEVM_TestMyMessageMenuClosed(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    UnRegisterForModEvent("EVM_ButtonMenuClosed") 
    int Button = a_numArg as int
    Debug.MessageBox("Button = " + Button)
EndEvent

;Text Input Menu ============================================================================================================================================================
Function DefaultInputMenu()
    RegisterForModEvent("EVM_TextInputClosed", "OnEVM_TextInputClosed")
    String NewInputString = ExtendedVanillaMenus.TextInput("Some Title", InputString)
EndFunction


Function CustomInputMenu()
    RegisterForModEvent("EVM_TextInputClosed", "OnEVM_TextInputClosed")
    String NewInputString = ExtendedVanillaMenus.TextInput("Some Title", InputString, AcceptText = "Alright", CancelText = "No Way", Width = 550, Height = 150, \
                                                            X = 600, Y = 250, ButtonAlpha = 0, BackgroundAlpha = 0, TextFieldAlpha = 75, TextColor = MyColor, TextBackgroundColor = 2155509, \ 
                                                            ShowTextBackground = true, MultiLine = false, align = 0, FontSize = 30, WaitForResult = true)
    
EndFunction

Event OnEVM_TextInputClosed(string a_eventName, string a_strArg, float a_numArg, form a_sender)
    UnRegisterForModEvent("EVM_TextInputClosed") 
    If a_numArg == 1 ;user accepted 
        InputString = a_strArg
        Debug.MessageBox(InputString) 
    elseif a_numArg == 0
        ;user canceled 
    Endif
EndEvent

;General Functions ============================================================================================================================================================
String Function GetLoremipsumNoPunctuation() Global 
	return "Lorem ipsum dolor sit amet consectetur adipiscing elit Etiam non urna velit Vivamus nisi mauris lacinia nec mollis ac elementum id arcu Nullam venenatis porta odio pharetra efficitur nisi lobortis vitae Maecenas eget orci ut enim euismod feugiat et sit amet arcu Sed eu lorem augue Ut libero risus euismod nec dui a auctor sodales magna In blandit aliquam viverra Vivamus vitae nisi molestie lobortis purus et finibus nulla Orci varius natoque penatibus et magnis dis parturient montes nascetur ridiculus mus Ut rhoncus id enim vel posuere Curabitur tincidunt vitae libero ut pulvinar Sed vitae quam eget orci egestas porttitor at in lectus Aenean tempus consectetur nulla sed rhoncus Nam vel felis erat"
EndFunction

String[] Function GetRandomWordsFromStringA(String s, int numOfWords, String Divider = " ") Global
	String[] words = StringUtil.Split(s, Divider) 
	
	int max = words.length - 1
	string[] sReturn
	
	if max == -1
		return sReturn
	endif
	
	sReturn = Utility.CreateStringArray(numOfWords)
	sReturn[0] = words[Utility.RandomInt(0, max)]
	
	int i = 1
	
	while i < numOfWords
		sReturn[i] = words[Utility.RandomInt(0, max)]
		i += 1
	EndWhile 
	
	return sReturn
EndFunction

;TweenMenu ===============================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================================
Sound Property UISkillsStop Auto
Sound Property UIMenuOk Auto
ImageSpaceModifier Property CGDragonBlurImod Auto

Function TweenMenuTest()
	;CGDragonBlurImod.Apply()
	Utility.Wait(0.5)
	RegisterForModEvent("ExtendedVanillaMenus_TweenMenuOptionHighlight", "OnExtendedVanillaMenus_TweenMenuOptionHighlight")
	RegisterForModEvent("ExtendedVanillaMenus_TweenMenuCloseStart", "OnExtendedVanillaMenus_TweenMenuCloseStart")
	RegisterForModEvent("ExtendedVanillaMenus_TweenMenuCloseEnd", "OnExtendedVanillaMenus_TweenMenuCloseEnd")
	ExtendedVanillaMenus.TweenMenu("UP", "Left", "Right", "Down", False) ;doesn't wait for result
EndFunction

;sent when a new option is highlighted by the user, by using the arrow keys or mouse.
Event OnExtendedVanillaMenus_TweenMenuOptionHighlight(string a_eventName, string a_strArg, float a_numArg, form a_sender)
	UISkillsStop.play(Game.GetPlayer())
	Debug.Notification(a_strArg + " " + (a_numArg as int))
EndEvent

;sent when starting to close the menu. a_strArg is text option selected, a_numArg is int option selected
Event OnExtendedVanillaMenus_TweenMenuCloseStart(string a_eventName, string a_strArg, float a_numArg, form a_sender)
	UIMenuOk.play(Game.GetPlayer())
	;CGDragonBlurImod.Remove()
	Debug.Notification(a_strArg + " " + (a_numArg as int))
EndEvent

;sent when finished closing the menu. a_strArg is text option selected, a_numArg is int option selected
Event OnExtendedVanillaMenus_TweenMenuCloseEnd(string a_eventName, string a_strArg, float a_numArg, form a_sender)
	;unregister events so they're not recieved when other scripts use the tween menu.
	UnRegisterForModEvent("ExtendedVanillaMenus_TweenMenuOptionHighlight")
	UnRegisterForModEvent("ExtendedVanillaMenus_TweenMenuCloseStart")
	UnRegisterForModEvent("ExtendedVanillaMenus_TweenMenuCloseEnd")
	
	;a_strArg is Text option selected
	;a_numArg: 0 = up, 1 = left, 2 = right, 3 = down, -1 = cancelled
	Debug.MessageBox(a_strArg + " " + (a_numArg as int))
EndEvent
