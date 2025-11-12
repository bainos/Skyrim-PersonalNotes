Scriptname ExtendedVanillaMenus_Examples extends Actor 
;example of how to use ExtendedVanillaMenus. This is also a preview. 
;Attach this script to a new quest, then in game press the left alt key to see how the menus work.

Int Property MyColor = 147979 Auto Hidden

Float Property MyFloat = 1.0 Auto Hidden ;for using only a single slider
String Property MyFloatSliderParams Auto Hidden ;single slider param string.

Float[] MySliderValues ;for multiple sliders in menu
String[] MySliders ;multiple slider params. Indexes should match with MySliderValues

String[] RandomWords ;for testing messageBox menu.

String InputString = ""

Event OnInit() 
    registerForModEvent("ExtendedVanillaMenus_Debug", "OmExtendedVanillaMenus_Debug")
    
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
    
    registerForKey(56) ;left alt
	Debug.Notification("Testing ExtendedVanillaMenus")
EndEvent 

Event OmExtendedVanillaMenus_Debug(string a_eventName, string a_strArg, float a_numArg, form a_sender)
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
	enchantment akEnchantment
	if akEnchantment.GetBaseEnchantment().PlayerKnows()
	
	Endif
	
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
    
    ;if CancelIsFirstButton == true, snaps to first button when pressing Tab or Escape. Otherwise snaps to last button.
    ;int Button = ExtendedVanillaMenus.MessageBox(sMessage = akMessage, Buttons = MyButtons, CancelIsFirstButton = true, WaitForResult = true)
    int Button = ExtendedVanillaMenus.MessageBox(akMessage, MyButtons, true) ;this is the same as above.
    
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
EndFunction


;color picker==========================================================================================================
Function SetMyColor() 
    int NewColor = ExtendedVanillaMenus.ColorPicker(MyColor, "My Color", "My Color Info")
    If NewColor > -1 ;user accepted new color
        MyColor = NewColor as int 
        string MyColorHex = DbMiscFunctions.ConvertintToHex(MyColor)
        ExtendedVanillaMenus.MessageBox("<font color='#" + MyColorHex + "'>" + "MyColor = " + MyColor)
    Endif
EndFunction

;single slider===========================================================================================================
Function SetMyFloat()
    MyFloat = ExtendedVanillaMenus.SliderMenu(MyFloatSliderParams, MyFloat, "$My Slider")
    Debug.MessageBox("MyFloat = " + MyFloat)
EndFunction

;Multiple Sliders ==============================================================================================================================================
Function SetMySliderValues()
    Float[] NewValues = ExtendedVanillaMenus.SliderMenuMult(SliderParams = MySliders, InitialValues = MySliderValues, TitleText = "My Sliders", AcceptText = "Alright", CancelText = "No Way")
    ;ExtendedVanillaMenus.SliderMenuMult(MySliders, MySliderValues, "My Sliders", "Alright", "No Way") ;this is the same as above.
    
    if NewValues.length == MySliderValues.length ;if NewValues array is the same length as our original float array, assume everything is a ok
        MySliderValues = NewValues 
        Debug.MessageBox("MySliderValues = " + MySliderValues)
    Endif
EndFunction

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

    int index = ExtendedVanillaMenus.ListMenu(Options, OptionInfos, "My Options", "Okay", "No Way", "[---]", 3)
    debug.MessageBox("List menu value = " + index)
EndFunction

;MessageBox menu ==============================================================================================================================================
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
	
    int listLength = extendedVanillaMenus.textInput("Set Number Of Buttons.") as int
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
    
    int Button = ExtendedVanillaMenus.MessageBox(akMessage, Buttons, false)
    Debug.MessageBox("Button = " + Button)
EndFunction

;Text Input Menu ============================================================================================================================================================
Function DefaultInputMenu()
    String NewInputString = ExtendedVanillaMenus.TextInput("Some Title", InputString)
    If NewInputString != ""
        InputString = NewInputString
        Debug.MessageBox(InputString) 
    Endif
EndFunction

Function CustomInputMenu()
    String NewInputString = ExtendedVanillaMenus.TextInput("Some Title", InputString, AcceptText = "Alright", CancelText = "No Way", Width = 550, Height = 150, \
                                                            X = 600, Y = 250, ButtonAlpha = 0, BackgroundAlpha = 0, TextFieldAlpha = 75, TextColor = MyColor, TextBackgroundColor = 2155509, \ 
                                                            ShowTextBackground = true, MultiLine = false, align = 0, FontSize = 30, WaitForResult = true)
    If NewInputString != ""
        InputString = NewInputString
        Debug.MessageBox(InputString) 
    Endif
EndFunction

;Tween Menu ============================================================================================================================================================
Function TweenMenuTest() 
	;returns: 0 = up, 1 = left, 2 = right, 3 = down, -1 = cancelled
	int result = ExtendedVanillaMenus.TweenMenu("UP", "Left", "Right", "Down")
	Debug.MessageBox(result + " chosen")
EndFunction

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