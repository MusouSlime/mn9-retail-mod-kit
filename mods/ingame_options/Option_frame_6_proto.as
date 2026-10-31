function AddOptionDataCategory(in_Index, in_GuideInfoArray)
{
   var _loc1_ = {};
   _loc1_.Index = in_Index;
   _loc1_.Type = OPTION_TYPE_CATEGORY;
   _loc1_.bEnable = true;
   _loc1_.GuideInfoArray = in_GuideInfoArray;
   _loc1_.InitFunction = InitFunctionCategory;
   _loc1_.SetDefaultFunction = SetDefaultFunctionCategory;
   _loc1_.CheckChangeFunction = CheckChangeFunctionCategory;
   _loc1_.RequestSetGuideFunction = RequestSetGuideFunctionCategory;
   _loc1_.StartFunction = StartFunctionCategory;
   _loc1_.EndFunction = EndFunctionCategory;
   _loc1_.LeftFunction = LeftFunctionCategory;
   _loc1_.RightFunction = RightFunctionCategory;
   _loc1_.DecideFunction = DecideFunctionCategory;
   _loc1_.SetFontFunction = SetFontFunctionCategory;
   return _loc1_;
}
function InitFunctionCategory(in_OptionData)
{
   if(in_OptionData.EndFunction(in_OptionData))
   {
      return true;
   }
   return false;
}
function SetDefaultFunctionCategory(in_OptionData)
{
   return true;
}
function CheckChangeFunctionCategory(in_OptionData)
{
   return false;
}
function RequestSetGuideFunctionCategory(in_OptionData)
{
   var _loc3_ = OptionCategoryIndex;
   var _loc2_ = in_OptionData.Index;
   if(in_OptionData.GuideInfoArray != undefined && in_OptionData.GuideInfoArray.length > 0)
   {
      _loc3_ = in_OptionData.GuideInfoArray[0].GuideType;
      _loc2_ = in_OptionData.GuideInfoArray[0].GuideID;
   }
   flash.external.ExternalInterface.call("RequestSetGuide",_loc3_,_loc2_);
}
function StartFunctionCategory(in_OptionData)
{
   in_OptionData.RequestSetGuideFunction(in_OptionData);
   window.gotoAndPlay("over" + (OptionCategoryIndex + 1));
   return true;
}
function EndFunctionCategory(in_OptionData)
{
   window.gotoAndPlay("normal" + (OptionCategoryIndex + 1));
   return true;
}
function LeftFunctionCategory(in_OptionData)
{
   DecCategoryIndex();
   flash.external.ExternalInterface.call("PlaySECursor");
   return true;
}
function RightFunctionCategory(in_OptionData)
{
   IncCategoryIndex();
   flash.external.ExternalInterface.call("PlaySECursor");
   return true;
}
function DecideFunctionCategory(in_OptionData)
{
   Down();
   return true;
}
function SetFontFunctionCategory(in_OptionData, in_FontName)
{
   return true;
}
function AddOptionDataOnOff(in_Index, in_MCName, in_InitData)
{
   var _loc1_ = {};
   _loc1_.Index = in_Index;
   _loc1_.MCName = in_MCName;
   _loc1_.Type = OPTION_TYPE_ONOFF;
   _loc1_.bOn = in_InitData.bOn;
   _loc1_.bOnInit = in_InitData.bOn;
   _loc1_.bOnDefault = in_InitData.bOnDefault;
   _loc1_.Name = in_InitData.Name;
   _loc1_.GuideInfoArray = in_InitData.GuideInfoArray;
   _loc1_.bEnable = true;
   if(in_InitData.RealTimeApplyFunctionName != undefined)
   {
      _loc1_.RealTimeApplyFunctionName = in_InitData.RealTimeApplyFunctionName;
      _loc1_.BackFunction = BackFunctionOnOff;
   }
   _loc1_.InitFunction = InitFunctionOnOff;
   _loc1_.SetDefaultFunction = SetDefaultFunctionOnOff;
   _loc1_.CheckChangeFunction = CheckChangeFunctionOnOff;
   _loc1_.RequestSetGuideFunction = RequestSetGuideFunctionOnOff;
   _loc1_.StartFunction = StartFunctionOnOff;
   _loc1_.EndFunction = EndFunctionOnOff;
   _loc1_.LeftFunction = LeftFunctionOnOff;
   _loc1_.RightFunction = RightFunctionOnOff;
   _loc1_.UpdateTextFunction = UpdateTextFunctionOnOff;
   _loc1_.ApplyFunction = ApplyFunctionOnOff;
   _loc1_.GetApplyOptionDataFunction = GetApplyOptionDataFunctionOnOff;
   _loc1_.SetFontFunction = SetFontFunctionOnOff;
   return _loc1_;
}
function StartChildOnOff(in_OptionData, in_Parent, in_ParentValue)
{
   in_OptionData.Parent = in_Parent;
   in_OptionData.ParentValue = in_ParentValue;
}
function InitFunctionOnOff(in_OptionData)
{
   if(in_OptionData.EndFunction(in_OptionData))
   {
      return in_OptionData.UpdateTextFunction(in_OptionData);
   }
   return false;
}
function SetDefaultFunctionOnOff(in_OptionData)
{
   in_OptionData.bOn = in_OptionData.bOnDefault;
   var _loc2_ = CategoryDataArray[OptionCategoryIndex];
   if(_loc2_.Index == in_OptionData.Index)
   {
      StartFunctionOnOff(in_OptionData);
   }
   else
   {
      EndFunctionOnOff(in_OptionData);
   }
   return true;
}
function CheckChangeFunctionOnOff(in_OptionData)
{
   return in_OptionData.bOn != in_OptionData.bOnInit;
}
function RequestSetGuideFunctionOnOff(in_OptionData)
{
   var _loc3_ = OptionCategoryIndex;
   var _loc2_ = in_OptionData.Index;
   if(in_OptionData.GuideInfoArray != undefined && in_OptionData.GuideInfoArray.length > 0)
   {
      _loc3_ = in_OptionData.GuideInfoArray[0].GuideType;
      _loc2_ = in_OptionData.GuideInfoArray[0].GuideID;
      if(in_OptionData.GuideInfoArray.length >= 2)
      {
         if(!in_OptionData.bOn)
         {
            _loc3_ = in_OptionData.GuideInfoArray[1].GuideType;
            _loc2_ = in_OptionData.GuideInfoArray[1].GuideID;
         }
      }
   }
   flash.external.ExternalInterface.call("RequestSetGuide",_loc3_,_loc2_);
}
function StartFunctionOnOff(in_OptionData)
{
   if(StartFunctionDefault(in_OptionData))
   {
      in_OptionData.RequestSetGuideFunction(in_OptionData);
      if(in_OptionData.bOn)
      {
         in_OptionData.OptionValueMC.gotoAndPlay("overON");
      }
      else
      {
         in_OptionData.OptionValueMC.gotoAndPlay("overOFF");
      }
      return true;
   }
   return false;
}
function EndFunctionOnOff(in_OptionData)
{
   if(EndFunctionDefault(in_OptionData))
   {
      if(in_OptionData.bOn)
      {
         in_OptionData.OptionValueMC.gotoAndPlay("normalON");
      }
      else
      {
         in_OptionData.OptionValueMC.gotoAndPlay("normalOFF");
      }
      return true;
   }
   return false;
}
function LeftFunctionOnOff(in_OptionData)
{
   if(OptionDataCheck("OptionStart.LeftFunctionOnOff",in_OptionData))
   {
      if(!in_OptionData.bOn)
      {
         in_OptionData.bOn = true;
         in_OptionData.OptionValueMC.gotoAndPlay("overON");
         OptionDataSetChangeFlag(true);
         if(in_OptionData.Parent != undefined && in_OptionData.Parent.Value != in_OptionData.ParentValue)
         {
            SetListValue(in_OptionData.Parent,in_OptionData.ParentValue);
         }
         if(in_OptionData.RealTimeApplyFunctionName != undefined)
         {
            flash.external.ExternalInterface.call(in_OptionData.RealTimeApplyFunctionName,in_OptionData.bOn,false);
         }
         flash.external.ExternalInterface.call("PlaySECursor");
      }
      return true;
   }
   return false;
}
function RightFunctionOnOff(in_OptionData)
{
   if(OptionDataCheck("OptionStart.RightFunctionOnOff",in_OptionData))
   {
      if(in_OptionData.bOn)
      {
         in_OptionData.bOn = false;
         in_OptionData.OptionValueMC.gotoAndPlay("overOFF");
         OptionDataSetChangeFlag(true);
         if(in_OptionData.Parent != undefined && in_OptionData.Parent.Value != in_OptionData.ParentValue)
         {
            trace("OptionOnOff.LeftFunctionOnOff: ParentValue = " + in_OptionData.Parent.Value + " > " + in_OptionData.ParentValue);
            SetListValue(in_OptionData.Parent,in_OptionData.ParentValue);
         }
         if(in_OptionData.RealTimeApplyFunctionName != undefined)
         {
            trace("OptionOnOff.RightFunctionOnOff: RealTimeApplyFunctionName = " + in_OptionData.RealTimeApplyFunctionName + ", bOn = " + in_OptionData.bOn);
            flash.external.ExternalInterface.call(in_OptionData.RealTimeApplyFunctionName,in_OptionData.bOn,false);
         }
         flash.external.ExternalInterface.call("PlaySECursor");
      }
      return true;
   }
   return false;
}
function SetOnOff(in_OptionData, in_bOn)
{
   if(OptionDataCheck("OptionStart.RightFunctionOnOff",in_OptionData))
   {
      if(in_OptionData.bOn != in_bOn)
      {
         in_OptionData.bOn = in_bOn;
         if(in_OptionData.bOn)
         {
            in_OptionData.OptionValueMC.gotoAndPlay("normalON");
         }
         else
         {
            in_OptionData.OptionValueMC.gotoAndPlay("normalOFF");
         }
         OptionDataSetChangeFlag(true);
      }
      return true;
   }
   return false;
}
function UpdateTextFunctionOnOff(in_OptionData)
{
   if(TextData != undefined && in_OptionData.OptionValueMC.textON != undefined)
   {
      in_OptionData.OptionValueMC.textON.textON.text = TextData.On;
      in_OptionData.OptionValueMC.textOFF.textOFF.text = TextData.Off;
      return true;
   }
   return false;
}
function UpdateTextFunctionNormalReverse(in_OptionData)
{
   if(TextData != undefined && in_OptionData.OptionValueMC.textON != undefined)
   {
      in_OptionData.OptionValueMC.textON.textON.text = TextData.Normal;
      in_OptionData.OptionValueMC.textOFF.textOFF.text = TextData.Reverse;
      return true;
   }
   return false;
}
function ApplyFunctionOnOff(in_OptionData)
{
   in_OptionData.bOnInit = in_OptionData.bOn;
   return true;
}
function GetApplyOptionDataFunctionOnOff(in_OptionData)
{
   trace("OptionOnOff( " + in_OptionData.Name + " ).GetApplyOptionDataFunctionOnOff: in_OptionData.bOn = " + in_OptionData.bOn);
   return in_OptionData.bOn;
}
function BackFunctionOnOff(in_OptionData)
{
   if(in_OptionData.RealTimeApplyFunctionName != undefined)
   {
      flash.external.ExternalInterface.call(in_OptionData.RealTimeApplyFunctionName,in_OptionData.bOn,true);
   }
   return true;
}
function SetFontFunctionOnOff(in_OptionData, in_FontName)
{
   CommonSetFont(in_OptionData.OptionNameMC.text.text,in_FontName);
   CommonSetFont(in_OptionData.OptionValueMC.textON.textON,in_FontName);
   CommonSetFont(in_OptionData.OptionValueMC.textOFF.textOFF,in_FontName);
   return true;
}
function AddOptionDataSlider(in_Index, in_MCName, in_InitData)
{
   var _loc1_ = {};
   _loc1_.Index = in_Index;
   _loc1_.MCName = in_MCName;
   _loc1_.Type = OPTION_TYPE_SLIDER;
   _loc1_.ValueMin = in_InitData.ValueMin;
   _loc1_.ValueMax = in_InitData.ValueMax;
   _loc1_.Value = in_InitData.Value;
   _loc1_.ValueInit = in_InitData.Value;
   _loc1_.ValueDefault = in_InitData.ValueDefault;
   _loc1_.ValueCount = in_InitData.ValueCount;
   _loc1_.Name = in_InitData.Name;
   _loc1_.GuideInfoArray = in_InitData.GuideInfoArray;
   _loc1_.bEnable = true;
   if(in_InitData.RealTimeApplyFunctionName != undefined)
   {
      _loc1_.RealTimeApplyFunctionName = in_InitData.RealTimeApplyFunctionName;
      _loc1_.BackFunction = BackFunctionSlider;
   }
   _loc1_.InitFunction = InitFunctionSlider;
   _loc1_.SetDefaultFunction = SetDefaultFunctionSlider;
   _loc1_.CheckChangeFunction = CheckChangeFunctionSlider;
   _loc1_.RequestSetGuideFunction = RequestSetGuideFunctionSlider;
   _loc1_.StartFunction = StartFunctionSlider;
   _loc1_.EndFunction = EndFunctionSlider;
   _loc1_.LeftFunction = LeftFunctionSlider;
   _loc1_.RightFunction = RightFunctionSlider;
   _loc1_.UpdateTextFunction = UpdateTextFunctionSlider;
   _loc1_.ApplyFunction = ApplyFunctionSlider;
   _loc1_.GetApplyOptionDataFunction = GetApplyOptionDataFunctionSlider;
   _loc1_.SetFontFunction = SetFontFunctionSlider;
   return _loc1_;
}
function InitFunctionSlider(in_OptionData)
{
   if(in_OptionData.EndFunction(in_OptionData))
   {
      SetSliderValue(in_OptionData,in_OptionData.Value);
      if(in_OptionData.RealTimeApplyFunctionName != undefined)
      {
         flash.external.ExternalInterface.call(in_OptionData.RealTimeApplyFunctionName,in_OptionData.Value,true);
      }
      return true;
   }
   return false;
}
function SetDefaultFunctionSlider(in_OptionData)
{
   SetSliderValue(in_OptionData,in_OptionData.ValueDefault);
   if(in_OptionData.RealTimeApplyFunctionName != undefined)
   {
      flash.external.ExternalInterface.call(in_OptionData.RealTimeApplyFunctionName,in_OptionData.Value,true);
   }
   return true;
}
function CheckChangeFunctionSlider(in_OptionData)
{
   return in_OptionData.Value != in_OptionData.ValueInit;
}
function RequestSetGuideFunctionSlider(in_OptionData)
{
   var _loc3_ = OptionCategoryIndex;
   var _loc2_ = in_OptionData.Index;
   if(in_OptionData.GuideInfoArray != undefined && in_OptionData.GuideInfoArray.length > 0)
   {
      _loc3_ = in_OptionData.GuideInfoArray[0].GuideType;
      _loc2_ = in_OptionData.GuideInfoArray[0].GuideID;
   }
   flash.external.ExternalInterface.call("RequestSetGuide",_loc3_,_loc2_);
}
function StartFunctionSlider(in_OptionData)
{
   if(StartFunctionDefault(in_OptionData))
   {
      in_OptionData.RequestSetGuideFunction(in_OptionData);
      if(in_OptionData.OptionValueMC.base == undefined)
      {
         trace("OptionStart.StartFunctionSlider: base is undefined.");
         return false;
      }
      in_OptionData.OptionValueMC.base.gotoAndPlay("over");
      return true;
   }
   return false;
}
function EndFunctionSlider(in_OptionData)
{
   if(EndFunctionDefault(in_OptionData))
   {
      if(in_OptionData.OptionValueMC.base == undefined)
      {
         trace("OptionStart.EndFunctionSlider: base is undefined.");
         return false;
      }
      in_OptionData.OptionValueMC.base.gotoAndPlay("normal");
      return true;
   }
   return false;
}
function LeftFunctionSlider(in_OptionData)
{
   var _loc3_;
   var _loc2_;
   if(OptionDataCheck("OptionStart.LeftFunctionSlider",in_OptionData))
   {
      _loc3_ = in_OptionData.Value;
      _loc2_ = DecSliderValue(in_OptionData);
      if(_loc2_ == NaN)
      {
         trace("OptionStart.LeftFunctionSlider: NewValue is NaN.");
         return false;
      }
      if(_loc3_ != _loc2_)
      {
         OptionDataSetChangeFlag(true);
         if(in_OptionData.RealTimeApplyFunctionName != undefined)
         {
            flash.external.ExternalInterface.call(in_OptionData.RealTimeApplyFunctionName,in_OptionData.Value,false);
         }
         flash.external.ExternalInterface.call("PlaySECursor");
      }
      return true;
   }
   return false;
}
function RightFunctionSlider(in_OptionData)
{
   var _loc3_;
   var _loc2_;
   if(OptionDataCheck("OptionStart.RightFunctionSlider",in_OptionData))
   {
      _loc3_ = in_OptionData.Value;
      _loc2_ = IncSliderValue(in_OptionData);
      if(_loc2_ == NaN)
      {
         trace("OptionStart.RightFunctionSlider: NewValue is NaN.");
         return false;
      }
      if(_loc3_ != _loc2_)
      {
         OptionDataSetChangeFlag(true);
         if(in_OptionData.RealTimeApplyFunctionName != undefined)
         {
            flash.external.ExternalInterface.call(in_OptionData.RealTimeApplyFunctionName,in_OptionData.Value,false);
         }
         flash.external.ExternalInterface.call("PlaySECursor");
      }
      return true;
   }
   return false;
}
function SetSliderValue(in_OptionData, in_Value)
{
   var _loc2_ = 16;
   var _loc5_ = 544;
   var _loc4_ = _loc5_ - _loc2_;
   var _loc3_;
   var _loc6_;
   if(OptionDataCheck("OptionStart.SetSliderValue",in_OptionData))
   {
      in_OptionData.Value = Clamp(in_OptionData.ValueMin,in_OptionData.ValueMax,in_Value);
      if(in_OptionData.OptionValueMC.marker == undefined)
      {
         trace("OptionStart.SetSliderValue: marker is undefined.");
         return in_OptionData.Value;
      }
      _loc3_ = in_OptionData.ValueMax - in_OptionData.ValueMin;
      _loc6_ = in_OptionData.Value - in_OptionData.ValueMin;
      if(_loc3_ == 0)
      {
         in_OptionData.OptionValueMC.marker._x = _loc2_ + _loc4_;
      }
      else
      {
         in_OptionData.OptionValueMC.marker._x = _loc2_ + _loc4_ * _loc6_ / _loc3_;
      }
      return in_OptionData.Value;
   }
   return NaN;
}
function IncSliderValue(in_OptionData)
{
   return SetSliderValue(in_OptionData,in_OptionData.Value + in_OptionData.ValueCount);
}
function DecSliderValue(in_OptionData)
{
   return SetSliderValue(in_OptionData,in_OptionData.Value - in_OptionData.ValueCount);
}
function UpdateTextFunctionSlider(in_OptionData)
{
   return true;
}
function ApplyFunctionSlider(in_OptionData)
{
   in_OptionData.ValueInit = in_OptionData.Value;
   return true;
}
function GetApplyOptionDataFunctionSlider(in_OptionData)
{
   trace("OptionSlider( " + in_OptionData.Name + " ).GetApplyOptionDataFunctionSlider: in_OptionData.Value = " + in_OptionData.Value);
   return in_OptionData.Value;
}
function BackFunctionSlider(in_OptionData)
{
   if(in_OptionData.RealTimeApplyFunctionName != undefined)
   {
      flash.external.ExternalInterface.call(in_OptionData.RealTimeApplyFunctionName,in_OptionData.ValueInit,true);
   }
   return true;
}
function SetFontFunctionSlider(in_OptionData, in_FontName)
{
   CommonSetFont(in_OptionData.OptionNameMC.text.text,in_FontName);
   return true;
}
function AddOptionDataList(in_Index, in_MCName, in_InitData)
{
   var _loc1_ = {};
   _loc1_.Index = in_Index;
   _loc1_.MCName = in_MCName;
   _loc1_.Type = OPTION_TYPE_LIST;
   _loc1_.ValueMin = in_InitData.ValueMin;
   _loc1_.ValueMax = in_InitData.ValueMax;
   var _loc2_;
   if(in_InitData.ListName)
   {
      _loc1_.ListName = in_InitData.ListName;
      if(_loc1_.ListName == "Number")
      {
         _loc1_.ListArray = new Array();
         _loc2_ = 0;
         while(_loc2_ <= _loc1_.ValueMax)
         {
            _loc1_.ListArray.push(String(_loc2_));
            _loc2_ = _loc2_ + 1;
         }
      }
      else
      {
         _loc1_.ListArray = TextData[in_InitData.ListName];
      }
   }
   else
   {
      _loc1_.ListArray = in_InitData.ListArray;
   }
   _loc1_.Value = in_InitData.Value;
   _loc1_.ValueInit = in_InitData.Value;
   _loc1_.ValueDefault = in_InitData.ValueDefault;
   _loc1_.bLoop = in_InitData.bLoop;
   _loc1_.Name = in_InitData.Name;
   _loc1_.GuideInfoArray = in_InitData.GuideInfoArray;
   _loc1_.bEnable = true;
   _loc1_.InitFunction = InitFunctionList;
   _loc1_.SetDefaultFunction = SetDefaultFunctionList;
   _loc1_.CheckChangeFunction = CheckChangeFunctionList;
   _loc1_.RequestSetGuideFunction = RequestSetGuideFunctionList;
   _loc1_.StartFunction = StartFunctionList;
   _loc1_.EndFunction = EndFunctionList;
   _loc1_.LeftFunction = LeftFunctionList;
   _loc1_.RightFunction = RightFunctionList;
   _loc1_.SetListValueFunction = SetListValue;
   _loc1_.UpdateTextFunction = UpdateTextFunctionList;
   _loc1_.ApplyFunction = ApplyFunctionList;
   _loc1_.GetApplyOptionDataFunction = GetApplyOptionDataFunctionList;
   _loc1_.SetFontFunction = SetFontFunctionList;
   trace("OptionList( " + _loc1_.Name + " ).AddOptionDataList: End");
   return _loc1_;
}
function StartParentList(in_OptionData, in_ChildArray, in_ParentValueArray)
{
   in_OptionData.ChildArray = in_ChildArray;
   in_OptionData.ParentValueArray = in_ParentValueArray;
   in_OptionData.SetListValueFunction = SetListValueParent;
}
function StartChildList(in_OptionData, in_Parent, in_ParentValue)
{
   in_OptionData.Parent = in_Parent;
   in_OptionData.ParentValue = in_ParentValue;
   in_OptionData.SetListValueFunction = SetListValueChild;
}
function InitFunctionList(in_OptionData)
{
   if(in_OptionData.EndFunction(in_OptionData))
   {
      in_OptionData.SetListValueFunction(in_OptionData,in_OptionData.Value);
      return true;
   }
   return false;
}
function SetDefaultFunctionList(in_OptionData)
{
   SetListValue(in_OptionData,in_OptionData.ValueDefault);
   return true;
}
function CheckChangeFunctionList(in_OptionData)
{
   return in_OptionData.Value != in_OptionData.ValueInit;
}
function RequestSetGuideFunctionList(in_OptionData)
{
   var _loc3_ = OptionCategoryIndex;
   var _loc2_ = in_OptionData.Index;
   if(in_OptionData.GuideInfoArray != undefined && in_OptionData.GuideInfoArray.length > 0)
   {
      if(in_OptionData.GuideInfoArray.length > in_OptionData.Value)
      {
         _loc3_ = in_OptionData.GuideInfoArray[in_OptionData.Value].GuideType;
         _loc2_ = in_OptionData.GuideInfoArray[in_OptionData.Value].GuideID;
      }
      else
      {
         _loc3_ = in_OptionData.GuideInfoArray[0].GuideType;
         _loc2_ = in_OptionData.GuideInfoArray[0].GuideID;
      }
   }
   flash.external.ExternalInterface.call("RequestSetGuide",_loc3_,_loc2_);
}
function StartFunctionList(in_OptionData)
{
   if(StartFunctionDefault(in_OptionData))
   {
      in_OptionData.RequestSetGuideFunction(in_OptionData);
      if(in_OptionData.OptionValueMC.base == undefined)
      {
         trace("OptionStart.StartFunctionList: base is undefined.");
         return false;
      }
      in_OptionData.OptionValueMC.base.gotoAndPlay("over");
      return true;
   }
   return false;
}
function EndFunctionList(in_OptionData)
{
   if(EndFunctionDefault(in_OptionData))
   {
      if(in_OptionData.OptionValueMC.base == undefined)
      {
         trace("OptionStart.EndFunctionList: base is undefined.");
         return false;
      }
      in_OptionData.OptionValueMC.base.gotoAndPlay("normal");
      return true;
   }
   return false;
}
function LeftFunctionList(in_OptionData)
{
   var _loc2_;
   var _loc1_;
   if(OptionDataCheck("OptionStart.LeftFunctionList",in_OptionData))
   {
      _loc2_ = in_OptionData.Value;
      _loc1_ = DecListValue(in_OptionData);
      if(_loc1_ == NaN)
      {
         trace("OptionStart.LeftFunctionList: NewValue is NaN.");
         return false;
      }
      if(_loc2_ != _loc1_)
      {
         OptionDataSetChangeFlag(true);
         flash.external.ExternalInterface.call("PlaySECursor");
      }
      return true;
   }
   return false;
}
function RightFunctionList(in_OptionData)
{
   var _loc2_;
   var _loc1_;
   if(OptionDataCheck("OptionStart.RightFunctionList",in_OptionData))
   {
      _loc2_ = in_OptionData.Value;
      _loc1_ = IncListValue(in_OptionData);
      if(_loc1_ == NaN)
      {
         trace("OptionStart.RightFunctionList: NewValue is NaN.");
         return false;
      }
      if(_loc2_ != _loc1_)
      {
         OptionDataSetChangeFlag(true);
         flash.external.ExternalInterface.call("PlaySECursor");
      }
      return true;
   }
   return false;
}
function SetListValue(in_OptionData, in_Value)
{
   if(OptionDataCheck("OptionStart.SetListValue",in_OptionData))
   {
      if(in_OptionData.bLoop)
      {
         if(in_Value < in_OptionData.ValueMin)
         {
            in_OptionData.Value = in_OptionData.ValueMax;
         }
         else if(in_Value > in_OptionData.ValueMax)
         {
            in_OptionData.Value = in_OptionData.ValueMin;
         }
         else
         {
            in_OptionData.Value = in_Value;
         }
      }
      else
      {
         in_OptionData.Value = Clamp(in_OptionData.ValueMin,in_OptionData.ValueMax,in_Value);
      }
      if(in_OptionData.OptionValueMC.text == undefined)
      {
         trace("OptionStart.SetListValue: text is undefined.");
      }
      else
      {
         in_OptionData.OptionValueMC.text.text.text = in_OptionData.ListArray[in_OptionData.Value];
      }
      if(in_OptionData.OptionValueMC.triangle == undefined)
      {
         trace("OptionStart( " + in_OptionData.Name + " ).SetListValue: triangle is undefined.");
      }
      else if(in_OptionData.ValueMax - in_OptionData.ValueMin > 0)
      {
         if(in_OptionData.bLoop)
         {
            in_OptionData.OptionValueMC.triangle.gotoAndPlay("LandR");
         }
         else if(in_OptionData.Value <= in_OptionData.ValueMin)
         {
            in_OptionData.OptionValueMC.triangle.gotoAndPlay("R");
         }
         else if(in_OptionData.Value >= in_OptionData.ValueMax)
         {
            in_OptionData.OptionValueMC.triangle.gotoAndPlay("L");
         }
         else
         {
            in_OptionData.OptionValueMC.triangle.gotoAndPlay("LandR");
         }
      }
      else
      {
         in_OptionData.OptionValueMC.triangle.gotoAndPlay("blank");
      }
      return in_OptionData.Value;
   }
   return NaN;
}
function IncListValue(in_OptionData)
{
   return in_OptionData.SetListValueFunction(in_OptionData,in_OptionData.Value + 1);
}
function DecListValue(in_OptionData)
{
   return in_OptionData.SetListValueFunction(in_OptionData,in_OptionData.Value - 1);
}
function SetListValueParent(in_OptionData, in_Value)
{
   var _loc5_ = in_OptionData.Value;
   var _loc4_ = SetListValue(in_OptionData,in_Value);
   var _loc1_;
   var _loc2_;
   if(_loc4_ != NaN && _loc5_ != _loc4_)
   {
      if(in_OptionData.ParentValueArray[_loc4_].ValueArray != undefined)
      {
         _loc1_ = 0;
         while(_loc1_ < in_OptionData.ChildArray.length)
         {
            _loc2_ = in_OptionData.ChildArray[_loc1_];
            switch(_loc2_.Type)
            {
               case OPTION_TYPE_LIST:
                  SetListValue(_loc2_,in_OptionData.ParentValueArray[_loc4_].ValueArray[_loc1_]);
                  break;
               case OPTION_TYPE_ONOFF:
                  SetOnOff(_loc2_,in_OptionData.ParentValueArray[_loc4_].ValueArray[_loc1_]);
            }
            _loc1_ = _loc1_ + 1;
         }
      }
   }
   return _loc4_;
}
function SetListValueChild(in_OptionData, in_Value)
{
   var _loc3_ = in_OptionData.Value;
   var _loc2_ = SetListValue(in_OptionData,in_Value);
   if(_loc2_ != NaN && _loc3_ != _loc2_)
   {
      if(in_OptionData.Parent != undefined && in_OptionData.Parent.Value != in_OptionData.ParentValue)
      {
         SetListValue(in_OptionData.Parent,in_OptionData.ParentValue);
      }
   }
   return _loc2_;
}
function UpdateTextFunctionList(in_OptionData)
{
   if(TextData != undefined && in_OptionData.ListName)
   {
      if(in_OptionData.ListName != "Number")
      {
         in_OptionData.ListArray = TextData[in_OptionData.ListName];
         if(in_OptionData.OptionValueMC.text != undefined)
         {
            in_OptionData.OptionValueMC.text.text.text = in_OptionData.ListArray[in_OptionData.Value];
         }
      }
   }
   return true;
}
function ApplyFunctionList(in_OptionData)
{
   in_OptionData.ValueInit = in_OptionData.Value;
   return true;
}
function GetApplyOptionDataFunctionList(in_OptionData)
{
   trace("OptionList( " + in_OptionData.Name + " ).GetApplyOptionDataFunctionList: in_OptionData.Value = " + in_OptionData.Value);
   return in_OptionData.Value;
}
function SetFontFunctionList(in_OptionData, in_FontName)
{
   CommonSetFont(in_OptionData.OptionNameMC.text.text,in_FontName);
   CommonSetFont(in_OptionData.OptionValueMC.text.text,in_FontName);
   return true;
}
function AddOptionDataWindowSize(in_Index, in_MCName, in_InitData)
{
   var _loc1_ = {};
   _loc1_.Index = in_Index;
   _loc1_.MCName = in_MCName;
   _loc1_.Type = OPTION_TYPE_WINDOW_SIZE;
   _loc1_.ScreenSizeListArray = in_InitData.ScreenSizeListArray;
   _loc1_.WideScreenSizeListArray = in_InitData.WideScreenSizeListArray;
   _loc1_.Value = in_InitData.Value;
   _loc1_.ValueInit = in_InitData.Value;
   _loc1_.ValueDefault = in_InitData.ValueDefault;
   if(InitData.bScreenSizeWide)
   {
      _loc1_.bWide = in_InitData.bWide;
      _loc1_.bWideInit = in_InitData.bWide;
      _loc1_.bWideDefault = in_InitData.bWideDefault;
   }
   else
   {
      _loc1_.bWide = false;
      _loc1_.bWideInit = false;
      _loc1_.bWideDefault = false;
   }
   if(_loc1_.bWide)
   {
      _loc1_.ListArray = _loc1_.WideScreenSizeListArray;
      _loc1_.OnValue = _loc1_.Value;
      _loc1_.OffValue = 0;
   }
   else
   {
      _loc1_.ListArray = _loc1_.ScreenSizeListArray;
      _loc1_.OnValue = 0;
      _loc1_.OffValue = _loc1_.Value;
   }
   _loc1_.ValueMin = 0;
   _loc1_.ValueMax = _loc1_.ListArray.length - 1;
   _loc1_.Name = in_InitData.Name;
   _loc1_.GuideInfoArray = in_InitData.GuideInfoArray;
   _loc1_.bEnable = true;
   _loc1_.InitFunction = InitFunctionWindowSize;
   _loc1_.SetDefaultFunction = SetDefaultFunctionWindowSize;
   _loc1_.CheckChangeFunction = CheckChangeFunctionWindowSize;
   _loc1_.RequestSetGuideFunction = RequestSetGuideFunctionWindowSize;
   _loc1_.StartFunction = StartFunctionWindowSize;
   _loc1_.EndFunction = EndFunctionWindowSize;
   _loc1_.LeftFunction = LeftFunctionWindowSize;
   _loc1_.RightFunction = RightFunctionWindowSize;
   _loc1_.SetListValueFunction = SetListValue;
   _loc1_.SubButtonFunction = SubButtonFunctionWindowSize;
   _loc1_.UpdateTextFunction = UpdateTextFunctionWindowSize;
   _loc1_.ApplyFunction = ApplyFunctionWindowSize;
   _loc1_.GetApplyOptionDataFunction = GetApplyOptionDataFunctionWindowSize;
   _loc1_.SetFontFunction = SetFontFunctionWindowSize;
   return _loc1_;
}
function InitFunctionWindowSize(in_OptionData)
{
   if(InitFunctionList(in_OptionData))
   {
      if(InitData.bScreenSizeWide)
      {
         if(in_OptionData.bWide)
         {
            window.video.widescreen.wideIcon.gotoAndPlay("ON");
         }
         else
         {
            window.video.widescreen.wideIcon.gotoAndPlay("OFF");
         }
      }
      else
      {
         window.video.widescreen._visible = false;
      }
      return true;
   }
   return false;
}
function SetDefaultFunctionWindowSize(in_OptionData)
{
   if(in_OptionData.bWide != in_OptionData.bWideDefault)
   {
      SubButtonFunctionWindowSize(in_OptionData);
   }
   SetListValue(in_OptionData,in_OptionData.ValueDefault);
   return true;
}
function CheckChangeFunctionWindowSize(in_OptionData)
{
   return in_OptionData.Value != in_OptionData.ValueInit || in_OptionData.bWide != in_OptionData.bWideInit;
}
function RequestSetGuideFunctionWindowSize(in_OptionData)
{
   var _loc3_ = OptionCategoryIndex;
   var _loc2_ = in_OptionData.Index;
   if(in_OptionData.GuideInfoArray != undefined && in_OptionData.GuideInfoArray.length > 0)
   {
      _loc3_ = in_OptionData.GuideInfoArray[0].GuideType;
      _loc2_ = in_OptionData.GuideInfoArray[0].GuideID;
   }
   flash.external.ExternalInterface.call("RequestSetGuide",_loc3_,_loc2_);
}
function StartFunctionWindowSize(in_OptionData)
{
   if(StartFunctionList(in_OptionData))
   {
      in_OptionData.RequestSetGuideFunction(in_OptionData);
      window.video.widescreen.wideButton.gotoAndPlay("over");
      return true;
   }
   return false;
}
function EndFunctionWindowSize(in_OptionData)
{
   if(EndFunctionList(in_OptionData))
   {
      window.video.widescreen.wideButton.gotoAndPlay("normal");
      return true;
   }
   return false;
}
function LeftFunctionWindowSize(in_OptionData)
{
   return LeftFunctionList(in_OptionData);
}
function RightFunctionWindowSize(in_OptionData)
{
   return RightFunctionList(in_OptionData);
}
function SubButtonFunctionWindowSize(in_OptionData)
{
   if(!InitData.bScreenSizeWide)
   {
      return undefined;
   }
   in_OptionData.bWide = !in_OptionData.bWide;
   if(in_OptionData.bWide)
   {
      in_OptionData.ListArray = in_OptionData.WideScreenSizeListArray;
      in_OptionData.OffValue = in_OptionData.Value;
      in_OptionData.Value = in_OptionData.OnValue;
      window.video.widescreen.wideIcon.gotoAndPlay("ON");
   }
   else
   {
      in_OptionData.ListArray = in_OptionData.ScreenSizeListArray;
      in_OptionData.OnValue = in_OptionData.Value;
      in_OptionData.Value = in_OptionData.OffValue;
      window.video.widescreen.wideIcon.gotoAndPlay("OFF");
   }
   in_OptionData.ValueMax = in_OptionData.ListArray.length - 1;
   SetListValue(in_OptionData,in_OptionData.Value);
   OptionDataSetChangeFlag(true);
   flash.external.ExternalInterface.call("PlaySECursor");
   return true;
}
function UpdateTextFunctionWindowSize(in_OptionData)
{
   return true;
}
function ApplyFunctionWindowSize(in_OptionData)
{
   in_OptionData.bWideInit = in_OptionData.bWide;
   in_OptionData.ValueInit = in_OptionData.Value;
   return true;
}
function GetApplyOptionDataFunctionWindowSize(in_OptionData)
{
   var _loc1_ = {};
   _loc1_.bWide = in_OptionData.bWide;
   _loc1_.Value = in_OptionData.Value;
   trace("OptionWindowSize( " + in_OptionData.Name + " ).GetApplyOptionDataFunctionWindowSize: ApplyOptionData.bWide = " + _loc1_.bWide);
   trace("OptionWindowSize( " + in_OptionData.Name + " ).GetApplyOptionDataFunctionWindowSize: ApplyOptionData.Value = " + _loc1_.Value);
   return _loc1_;
}
function SetFontFunctionWindowSize(in_OptionData, in_FontName)
{
   CommonSetFont(in_OptionData.OptionNameMC.text.text,in_FontName);
   CommonSetFont(in_OptionData.OptionValueMC.text.text,in_FontName);
   return true;
}
function CreateButtonData(in_ButtonPath, in_bFullPath, in_Function)
{
   var _loc5_ = {};
   var _loc4_;
   var _loc3_;
   var _loc2_;
   if(in_bFullPath)
   {
      _loc4_ = in_ButtonPath.split(".");
      _loc3_ = this[_loc4_[0]];
      _loc2_ = 1;
      while(_loc2_ < _loc4_.length)
      {
         _loc3_ = _loc3_[_loc4_[_loc2_]];
         _loc2_ = _loc2_ + 1;
      }
      _loc5_.ButtonMC = _loc3_;
   }
   else
   {
      _loc5_.MCName = in_ButtonPath;
   }
   _loc5_.bEnable = true;
   _loc5_.Function = in_Function;
   return _loc5_;
}
function CreateDummyButtonData()
{
   var _loc1_ = {};
   _loc1_.bEnable = false;
   return _loc1_;
}
function AddOptionDataButton(in_Index, in_ButtonType, in_ButtonDataArray, in_Value, in_GuideInfoArray)
{
   var _loc1_ = {};
   _loc1_.Index = in_Index;
   _loc1_.Type = OPTION_TYPE_BUTTON;
   _loc1_.ButtonType = in_ButtonType;
   _loc1_.ButtonDataArray = in_ButtonDataArray;
   _loc1_.Value = in_Value;
   _loc1_.DefaultValue = in_Value;
   _loc1_.bEnable = true;
   _loc1_.bLoopEnable = false;
   _loc1_.GuideInfoArray = in_GuideInfoArray;
   _loc1_.InitFunction = InitFunctionButton;
   _loc1_.SetDefaultFunction = SetDefaultFunctionButton;
   _loc1_.CheckChangeFunction = CheckChangeFunctionButton;
   _loc1_.RequestSetGuideFunction = RequestSetGuideFunctionButton;
   _loc1_.StartFunction = StartFunctionButton;
   _loc1_.EndFunction = EndFunctionButton;
   _loc1_.LeftFunction = LeftFunctionButton;
   _loc1_.RightFunction = RightFunctionButton;
   _loc1_.DecideFunction = DecideFunctionButton;
   _loc1_.SetValueFunction = SetValueFunctionButton;
   _loc1_.SetFontFunction = SetFontFunctionButton;
   return _loc1_;
}
function AddOptionDataSystemButton(in_Index, in_ButtonDataArray, in_Value, in_GuideInfoArray)
{
   return AddOptionDataButton(in_Index,BUTTON_TYPE_SYSTEM,in_ButtonDataArray,in_Value,in_GuideInfoArray);
}
function ButtonDataCheck(in_OptionData, in_ButtonIndex)
{
   if(in_OptionData == undefined)
   {
      trace("OptionButton.ButtonDataCheck: in_OptionData is undefined.");
      return false;
   }
   var _loc1_ = in_OptionData.ButtonDataArray[in_OptionData.Value];
   if(_loc1_ == undefined)
   {
      trace("OptionButton.ButtonDataCheck: ButtonData[ " + in_OptionData.Value + " ] is undefined.");
      return false;
   }
   var _loc3_ = _loc1_.ButtonMC;
   if(_loc3_ == undefined)
   {
      trace("OptionButton.ButtonDataCheck: ButtonMC is undefined.");
      return false;
   }
   return true;
}
function SetDefaultFunctionButton(in_OptionData)
{
   return true;
}
function CheckChangeFunctionButton(in_OptionData)
{
   return false;
}
function InitFunctionButton(in_OptionData)
{
   var _loc2_;
   var _loc1_;
   if(in_OptionData.EndFunction(in_OptionData))
   {
      _loc2_ = {};
      _loc1_ = 0;
      while(_loc1_ < in_OptionData.ButtonDataArray.length)
      {
         _loc2_ = in_OptionData.ButtonDataArray[_loc1_];
         if(_loc2_.bEnable)
         {
            _loc2_.ButtonMC.gotoAndPlay("up");
         }
         else
         {
            _loc2_.ButtonMC.gotoAndPlay("disabled");
         }
         _loc1_ = _loc1_ + 1;
      }
      return true;
   }
   return false;
}
function RequestSetGuideFunctionButton(in_OptionData)
{
   var _loc3_ = OptionCategoryIndex;
   var _loc2_ = in_OptionData.Index;
   if(in_OptionData.GuideInfoArray != undefined && in_OptionData.GuideInfoArray.length > 0)
   {
      if(in_OptionData.GuideInfoArray.length > in_OptionData.Value)
      {
         _loc3_ = in_OptionData.GuideInfoArray[in_OptionData.Value].GuideType;
         _loc2_ = in_OptionData.GuideInfoArray[in_OptionData.Value].GuideID;
      }
      else
      {
         _loc3_ = in_OptionData.GuideInfoArray[0].GuideType;
         _loc2_ = in_OptionData.GuideInfoArray[0].GuideID;
      }
   }
   flash.external.ExternalInterface.call("RequestSetGuide",_loc3_,_loc2_);
}
function StartFunctionButton(in_OptionData)
{
   if(!ButtonDataCheck(in_OptionData,in_OptionData.Value))
   {
      trace("OptionButton.StartFunctionButton: ButtonDataCheck Failure.");
      return false;
   }
   var _loc0_;
   if((_loc0_ = in_OptionData.ButtonType) !== BUTTON_TYPE_SYSTEM)
   {
      in_OptionData.Value = OptionButtonValue;
      if(in_OptionData.Value >= in_OptionData.ButtonDataArray.length)
      {
         in_OptionData.Value = in_OptionData.ButtonDataArray.length - 1;
      }
      i = in_OptionData.Value;
      while(i >= 0)
      {
         in_OptionData.Value = i;
         if(in_OptionData.ButtonDataArray[i].ButtonMC._visible)
         {
            break;
         }
         i--;
      }
   }
   else if(CategoryDataArray[OptionCategoryIndex].bChange)
   {
      in_OptionData.Value = 1;
   }
   else
   {
      in_OptionData.Value = 0;
   }
   in_OptionData.RequestSetGuideFunction(in_OptionData);
   var i = 0;
   var _loc3_;
   var _loc2_;
   while(i < in_OptionData.ButtonDataArray.length)
   {
      _loc3_ = in_OptionData.ButtonDataArray[i];
      _loc2_ = _loc3_.ButtonMC;
      if(_loc3_.bEnable)
      {
         if(i == in_OptionData.Value)
         {
            _loc2_.gotoAndPlay("over");
         }
         else
         {
            _loc2_.gotoAndPlay("up");
         }
      }
      else
      {
         _loc2_.gotoAndPlay("disabled");
      }
      i++;
   }
   return true;
}
function EndFunctionButton(in_OptionData)
{
   if(!ButtonDataCheck(in_OptionData,in_OptionData.Value))
   {
      trace("OptionButton.EndFunctionButton: ButtonDataCheck Failure.");
      return false;
   }
   var _loc2_ = in_OptionData.ButtonDataArray[in_OptionData.Value];
   var _loc3_ = _loc2_.ButtonMC;
   _loc3_.gotoAndPlay("up");
   if(in_OptionData.ButtonType != BUTTON_TYPE_SYSTEM)
   {
      OptionButtonValue = in_OptionData.Value;
   }
   return true;
}
function LeftFunctionButton(in_OptionData)
{
   if(in_OptionData == undefined)
   {
      trace("OptionButton.LeftFunctionButton: in_OptionData is undefined.");
      return false;
   }
   var _loc2_ = in_OptionData.Value;
   GotoPrevButton(in_OptionData);
   return true;
}
function RightFunctionButton(in_OptionData)
{
   if(in_OptionData == undefined)
   {
      trace("OptionButton.RightFunctionButton: in_OptionData is undefined.");
      return false;
   }
   var _loc2_ = in_OptionData.Value;
   GotoNextButton(in_OptionData);
   return true;
}
function DecideFunctionButton(in_OptionData)
{
   if(!ButtonDataCheck(in_OptionData,in_OptionData.Value))
   {
      trace("OptionButton.DecideFunctionButton: ButtonDataCheck Failure.");
      return false;
   }
   var _loc2_ = in_OptionData.ButtonDataArray[in_OptionData.Value];
   var _loc3_ = _loc2_.ButtonMC;
   _loc3_.gotoAndPlay("down");
   InputSetAllKeyDataEnable(false);
   trace("OptionButton.DecideFunctionButton: call InputSetAllKeyDataEnable( false )");
   flash.external.ExternalInterface.call("PlaySEDecide");
   return true;
}
function GotoNextButton(in_OptionData)
{
   var _loc4_ = in_OptionData.Value;
   var _loc3_ = _loc4_;
   var _loc1_ = _loc4_;
   while(true)
   {
      _loc1_ = _loc1_ + 1;
      if(_loc1_ >= in_OptionData.ButtonDataArray.length)
      {
         if(!in_OptionData.bLoopEnable)
         {
            break;
         }
         _loc1_ -= in_OptionData.ButtonDataArray.length;
      }
      if(in_OptionData.ButtonDataArray[_loc1_].bEnable)
      {
         _loc3_ = _loc1_;
         break;
      }
   }
   if(_loc4_ != _loc3_)
   {
      in_OptionData.SetValueFunction(in_OptionData,_loc3_);
   }
   return true;
}
function GotoPrevButton(in_OptionData)
{
   var _loc4_ = in_OptionData.Value;
   var _loc3_ = _loc4_;
   var _loc1_ = _loc4_;
   while(true)
   {
      _loc1_ = _loc1_ - 1;
      if(_loc1_ < 0)
      {
         if(!in_OptionData.bLoopEnable)
         {
            break;
         }
         _loc1_ += in_OptionData.ButtonDataArray.length;
      }
      if(in_OptionData.ButtonDataArray[_loc1_].bEnable)
      {
         _loc3_ = _loc1_;
         break;
      }
   }
   if(_loc4_ != _loc3_)
   {
      in_OptionData.SetValueFunction(in_OptionData,_loc3_);
   }
   return true;
}
function SetValueFunctionButton(in_OptionData, in_Value)
{
   var _loc3_ = in_OptionData.Value;
   var _loc2_ = in_Value;
   in_OptionData.Value = _loc2_;
   in_OptionData.ButtonDataArray[_loc3_].ButtonMC.gotoAndPlay("up");
   in_OptionData.ButtonDataArray[_loc2_].ButtonMC.gotoAndPlay("over");
   if(in_OptionData.ButtonType != BUTTON_TYPE_SYSTEM)
   {
      OptionButtonValue = in_OptionData.Value;
   }
   in_OptionData.RequestSetGuideFunction(in_OptionData);
   flash.external.ExternalInterface.call("PlaySECursor");
   return true;
}
function ButtonEnd()
{
   trace("OptionButton.ButtonEnd");
   var _loc3_ = CategoryDataArray[OptionCategoryIndex];
   var _loc1_ = _loc3_.OptionDataArray[_loc3_.Index];
   var _loc2_;
   if(_loc1_.Type == OPTION_TYPE_BUTTON)
   {
      _loc2_ = _loc1_.ButtonDataArray[_loc1_.Value];
      return _loc2_.Function(_loc2_);
   }
   return false;
}
function SetFontFunctionButton(in_OptionData, in_FontName)
{
   var _loc1_ = 0;
   var _loc2_;
   var _loc3_;
   while(_loc1_ < in_OptionData.ButtonDataArray.length)
   {
      _loc2_ = in_OptionData.ButtonDataArray[_loc1_];
      _loc3_ = _loc2_.ButtonMC;
      CommonSetFont(_loc3_.text.text,in_FontName);
      _loc1_ = _loc1_ + 1;
   }
   return true;
}
function InitVideoData()
{
   var _loc4_ = new Array();
   var _loc9_ = {};
   var _loc1_ = InitData.OptionData.Video;
   var _loc7_;
   var _loc8_ = new Array();
   var _loc5_;
   _loc4_.push(AddOptionDataCategory(_loc4_.length,_loc1_.CategoryGuideInfoArray));
   if(_loc1_.ScreenType != undefined)
   {
      _loc4_.push(AddOptionDataList(_loc4_.length,"video1",_loc1_.ScreenType));
   }
   if(_loc1_.ScreenSize != undefined)
   {
      _loc4_.push(AddOptionDataWindowSize(_loc4_.length,"video2",_loc1_.ScreenSize));
   }
   if(_loc1_.ScreenPercentage != undefined)
   {
      _loc4_.push(AddOptionDataSlider(_loc4_.length,"video3",_loc1_.ScreenPercentage));
   }
   if(_loc1_.Gamma != undefined)
   {
      _loc4_.push(AddOptionDataSlider(_loc4_.length,"video4",_loc1_.Gamma));
   }
   if(_loc1_.bVSync != undefined)
   {
      _loc4_.push(AddOptionDataOnOff(_loc4_.length,"video5",_loc1_.bVSync));
   }
   if(_loc1_.GraphicQuality != undefined)
   {
      _loc4_.push(AddOptionDataList(_loc4_.length,"video6",_loc1_.GraphicQuality));
      _loc7_ = _loc4_[_loc4_.length - 1];
   }
   if(_loc1_.TextureQuality != undefined)
   {
      _loc4_.push(AddOptionDataList(_loc4_.length,"video7",_loc1_.TextureQuality));
      _loc5_ = _loc4_[_loc4_.length - 1];
      _loc8_.push(_loc5_);
      StartChildList(_loc5_,_loc7_,5);
   }
   if(_loc1_.MeshQuality != undefined)
   {
      _loc4_.push(AddOptionDataList(_loc4_.length,"video8",_loc1_.MeshQuality));
      _loc5_ = _loc4_[_loc4_.length - 1];
      _loc8_.push(_loc5_);
      StartChildList(_loc5_,_loc7_,5);
   }
   if(_loc1_.AntiAliasing != undefined)
   {
      _loc4_.push(AddOptionDataList(_loc4_.length,"video9",_loc1_.AntiAliasing));
      _loc5_ = _loc4_[_loc4_.length - 1];
      _loc8_.push(_loc5_);
      StartChildList(_loc5_,_loc7_,5);
   }
   if(_loc1_.Anisotropic != undefined)
   {
      _loc4_.push(AddOptionDataList(_loc4_.length,"video10",_loc1_.Anisotropic));
      _loc5_ = _loc4_[_loc4_.length - 1];
      _loc8_.push(_loc5_);
      StartChildList(_loc5_,_loc7_,5);
   }
   if(_loc1_.ShadowQuality != undefined)
   {
      _loc4_.push(AddOptionDataList(_loc4_.length,"video11",_loc1_.ShadowQuality));
      _loc5_ = _loc4_[_loc4_.length - 1];
      _loc8_.push(_loc5_);
      StartChildList(_loc5_,_loc7_,5);
   }
   if(_loc1_.bBloom != undefined)
   {
      _loc4_.push(AddOptionDataOnOff(_loc4_.length,"video13",_loc1_.bBloom));
      _loc5_ = _loc4_[_loc4_.length - 1];
      _loc8_.push(_loc5_);
      StartChildOnOff(_loc5_,_loc7_,5);
   }
   if(_loc1_.bMotionBlur != undefined)
   {
      _loc4_.push(AddOptionDataOnOff(_loc4_.length,"video14",_loc1_.bMotionBlur));
      _loc5_ = _loc4_[_loc4_.length - 1];
      _loc8_.push(_loc5_);
      StartChildOnOff(_loc5_,_loc7_,5);
   }
   var _loc6_;
   var _loc2_;
   var _loc3_;
   if(_loc1_.GraphicQuality != undefined)
   {
      _loc6_ = new Array();
      _loc2_ = 0;
      while(_loc2_ < _loc1_.GraphicQualityData.length)
      {
         _loc3_ = new Array();
         _loc3_.push(_loc1_.GraphicQualityData[_loc2_].TextureQuality);
         _loc3_.push(_loc1_.GraphicQualityData[_loc2_].MeshQuality);
         _loc3_.push(_loc1_.GraphicQualityData[_loc2_].AntiAliasing);
         _loc3_.push(_loc1_.GraphicQualityData[_loc2_].Anisotropic);
         _loc3_.push(_loc1_.GraphicQualityData[_loc2_].ShadowQuality);
         _loc3_.push(_loc1_.GraphicQualityData[_loc2_].bBloom);
         _loc3_.push(_loc1_.GraphicQualityData[_loc2_].bMotionBlur);
         _loc6_[_loc2_] = {};
         _loc6_[_loc2_].ValueArray = _loc3_;
         _loc2_ = _loc2_ + 1;
      }
      StartParentList(_loc7_,_loc8_,_loc6_);
   }
   var _loc10_ = CreateButtonDataArray(_loc1_.bInitializeEnable);
   _loc5_ = AddOptionDataSystemButton(_loc4_.length,_loc10_,0,_loc1_.SystemButtonGuideInfoArray);
   _loc4_.push(_loc5_);
   _loc9_.SystemButtonData = _loc5_;
   _loc9_.OptionDataArray = _loc4_;
   _loc9_.Index = 0;
   _loc9_.bChange = false;
   _loc9_.InitData = _loc1_;
   CategoryDataArray[OPTION_CATEGORY_VIDEO] = _loc9_;
}
function InitAudioData()
{
   var _loc2_ = new Array();
   var _loc4_ = {};
   var _loc1_ = InitData.OptionData.Audio;
   var _loc3_ = 0;
   trace("OptionStart.InitAudioData: InitAudioDataObj = " + _loc1_);
   _loc2_.push(AddOptionDataCategory(_loc2_.length,_loc1_.CategoryGuideInfoArray));
   if(_loc1_.BGMVolume != undefined)
   {
      _loc2_.push(AddOptionDataSlider(_loc2_.length,"audio" + (_loc3_ = _loc3_ + 1),_loc1_.BGMVolume));
   }
   if(_loc1_.SEVolume != undefined)
   {
      _loc2_.push(AddOptionDataSlider(_loc2_.length,"audio" + (_loc3_ = _loc3_ + 1),_loc1_.SEVolume));
   }
   if(_loc1_.VoiceVolume != undefined)
   {
      _loc2_.push(AddOptionDataSlider(_loc2_.length,"audio" + (_loc3_ = _loc3_ + 1),_loc1_.VoiceVolume));
   }
   if(_loc1_.bEightBitSound != undefined)
   {
      _loc2_.push(AddOptionDataOnOff(_loc2_.length,"audio" + (_loc3_ = _loc3_ + 1),_loc1_.bEightBitSound));
   }
   if(_loc1_.SoundTestBGM != undefined)
   {
      OptionData = AddOptionDataList(_loc2_.length,"audio" + (_loc3_ = _loc3_ + 1),_loc1_.SoundTestBGM);
      SetSoundTestObject(OptionData,_loc1_.SoundTestBGM.SoundTestType);
      _loc2_.push(OptionData);
   }
   if(_loc1_.SoundTestSE1 != undefined)
   {
      OptionData = AddOptionDataList(_loc2_.length,"audio" + (_loc3_ = _loc3_ + 1),_loc1_.SoundTestSE1);
      SetSoundTestObject(OptionData,_loc1_.SoundTestSE1.SoundTestType);
      _loc2_.push(OptionData);
   }
   if(_loc1_.SoundTestSE2 != undefined)
   {
      OptionData = AddOptionDataList(_loc2_.length,"audio" + (_loc3_ = _loc3_ + 1),_loc1_.SoundTestSE2);
      SetSoundTestObject(OptionData,_loc1_.SoundTestSE2.SoundTestType);
      _loc2_.push(OptionData);
   }
   if(_loc1_.SoundTestVoice != undefined)
   {
      OptionData = AddOptionDataList(_loc2_.length,"audio" + (_loc3_ = _loc3_ + 1),_loc1_.SoundTestVoice);
      SetSoundTestObject(OptionData,_loc1_.SoundTestVoice.SoundTestType);
      _loc2_.push(OptionData);
   }
   var _loc5_ = CreateButtonDataArray(_loc1_.bInitializeEnable);
   OptionData = AddOptionDataSystemButton(_loc2_.length,_loc5_,0,_loc1_.SystemButtonGuideInfoArray);
   _loc2_.push(OptionData);
   _loc4_.SystemButtonData = OptionData;
   _loc4_.OptionDataArray = _loc2_;
   _loc4_.Index = 0;
   _loc4_.bChange = false;
   _loc4_.InitData = _loc1_;
   CategoryDataArray[OPTION_CATEGORY_AUDIO] = _loc4_;
}
function InitOtherData()
{
   var _loc1_ = new Array();
   var _loc4_ = {};
   var _loc3_ = InitData.OptionData.Other;
   var _loc2_;
   _loc1_.push(AddOptionDataCategory(_loc1_.length,_loc3_.CategoryGuideInfoArray));
   var _loc6_;
   if(_loc3_.CalibrationGuideInfoArray != undefined)
   {
      _loc6_ = CreateButtonData("calibration",false,ButtonCalibration);
      _loc1_.push(AddOptionDataButton(_loc1_.length,BUTTON_TYPE_OTHERS,[_loc6_],0,_loc3_.CalibrationGuideInfoArray));
   }
   if(_loc3_.Language != undefined)
   {
      _loc2_ = AddOptionDataList(_loc1_.length,"etc1",_loc3_.Language);
      _loc2_.FontList = TextData.LanguageFontList;
      _loc2_.SetListValueFunction = SetLanguage;
      _loc2_.SetFontFunction = SetFontFunctionLanguage;
      _loc1_.push(_loc2_);
   }
   if(_loc3_.VoiceLanguage != undefined)
   {
      _loc2_ = AddOptionDataList(_loc1_.length,"etc4",_loc3_.VoiceLanguage);
      _loc2_.FontList = TextData.VoiceLanguageFontList;
      _loc2_.SetListValueFunction = SetListValueLanguage;
      _loc2_.SetFontFunction = SetFontFunctionLanguage;
      _loc1_.push(_loc2_);
   }
   if(_loc3_.bDecideCancelButtonNormal != undefined)
   {
      _loc2_ = AddOptionDataOnOff(_loc1_.length,"etc2",_loc3_.bDecideCancelButtonNormal);
      _loc2_.UpdateTextFunction = UpdateTextFunctionNormalReverse;
      _loc1_.push(_loc2_);
   }
   if(_loc3_.RestNum != undefined)
   {
      _loc1_.push(AddOptionDataList(_loc1_.length,"etc3",_loc3_.RestNum));
   }
   // --- MN9 Patcher prototype rows ---
   _loc2_ = AddOptionDataOnOff(_loc1_.length,"mn9mouse",{bOn:true,bOnDefault:true,Name:"Free Mouse",GuideInfoArray:undefined});
   _loc2_.UpdateTextFunction = UpdateTextFunctionOnOff;
   _loc1_.push(_loc2_);
   _loc2_ = AddOptionDataOnOff(_loc1_.length,"mn9movies",{bOn:false,bOnDefault:false,Name:"Intro Movies",GuideInfoArray:undefined});
   _loc2_.UpdateTextFunction = UpdateTextFunctionOnOff;
   _loc1_.push(_loc2_);
   _loc2_ = AddOptionDataOnOff(_loc1_.length,"mn9voice",{bOn:false,bOnDefault:false,Name:"Steam Voice",GuideInfoArray:undefined});
   _loc2_.UpdateTextFunction = UpdateTextFunctionOnOff;
   _loc1_.push(_loc2_);
   var _loc5_ = CreateButtonDataArray(_loc3_.bInitializeEnable);
   _loc2_ = AddOptionDataSystemButton(_loc1_.length,_loc5_,0,_loc3_.SystemButtonGuideInfoArray);
   _loc1_.push(_loc2_);
   _loc4_.SystemButtonData = _loc2_;
   _loc4_.OptionDataArray = _loc1_;
   _loc4_.Index = 0;
   _loc4_.bChange = false;
   _loc4_.InitData = _loc3_;
   CategoryDataArray[OPTION_CATEGORY_OTHER] = _loc4_;
}
function Init()
{
   trace("OptionStart.Init");
   LanguageInitData = flash.external.ExternalInterface.call("GetLanguageInitData");
   InitData = flash.external.ExternalInterface.call("GetInitData");
   TextData = flash.external.ExternalInterface.call("GetTextData");
   flash.external.ExternalInterface.call("SetText");
   CategoryDataArray = new Array(OPTION_CATEGORY_NUM);
   trace("Option.Init: CategoryDataArray.length = " + CategoryDataArray.length);
   InitVideoData();
   InitAudioData();
   InitOtherData();
   bInitCategoryData = false;
   SetCategoryIndex(InitData.StartCategoryIndex);
   trace("Option.Init: InitData.bRestart = " + InitData.bRestart);
   if(InitData.bRestart)
   {
      gotoAndPlay(15);
   }
}
function ChangeLanguage()
{
   trace("OptionStart.ChangeLanguage()");
   LanguageInitData = flash.external.ExternalInterface.call("GetLanguageInitData");
   if(LanguageInitData != undefined)
   {
      SetFont(LanguageInitData.FontName);
   }
}
function CreateButtonDataArray(in_bInitializeEnable)
{
   var _loc1_ = new Array();
   var _loc2_;
   _loc1_.push(CreateButtonData("window.back",true,ButtonBack));
   _loc1_.push(CreateButtonData("window.apply",true,ButtonApply));
   _loc2_ = CreateButtonData("window.defaultSetting",true,ButtonInitialize);
   _loc2_.bEnable = in_bInitializeEnable;
   _loc1_.push(_loc2_);
   return _loc1_;
}
function SetSoundTestObject(in_OptionData, in_SoundTestType)
{
   in_OptionData.LeftFunction = LeftFunctionSoundTest;
   in_OptionData.RightFunction = RightFunctionSoundTest;
   in_OptionData.SetDefaultFunction = SetDefaultFunctionSoundTest;
   in_OptionData.CheckChangeFunction = CheckChangeFunctionSoundTest;
   in_OptionData.StartFunction = StartFunctionSoundTest;
   in_OptionData.EndFunction = EndFunctionSoundTest;
   in_OptionData.DecideFunction = DecideFunctionSoundTest;
   in_OptionData.SoundTestType = in_SoundTestType;
   SoundTestObjectArray.push(in_OptionData);
}
function StartFunctionDefault(in_OptionData)
{
   if(OptionDataCheck("OptionStart.StartFunctionDefault",in_OptionData))
   {
      in_OptionData.OptionNameMC.gotoAndPlay("over");
      return true;
   }
   return false;
}
function EndFunctionDefault(in_OptionData)
{
   if(OptionDataCheck("OptionStart.EndFunctionDefault",in_OptionData))
   {
      in_OptionData.OptionNameMC.gotoAndPlay("up");
      return true;
   }
   return false;
}
function SetLanguage(in_OptionData, in_Value)
{
   var _loc1_ = SetListValueLanguage(in_OptionData,in_Value);
   bChangeLanguage = true;
   return _loc1_;
}
function SetListValueLanguage(in_OptionData, in_Value)
{
   var _loc1_ = SetListValue(in_OptionData,in_Value);
   trace("OptionStart.SetListValueLanguage(): NewValue = " + _loc1_);
   trace("OptionStart.SetListValueLanguage(): FontList.length =  = " + in_OptionData.FontList.length);
   trace("OptionStart.SetListValueLanguage(): FontList[ " + _loc1_ + " ] = " + in_OptionData.FontList[_loc1_]);
   CommonSetFont(in_OptionData.OptionValueMC.text.text,in_OptionData.FontList[_loc1_]);
   return _loc1_;
}
function SetFontFunctionLanguage(in_OptionData, in_FontName)
{
   CommonSetFont(in_OptionData.OptionNameMC.text.text,in_FontName);
   trace("OptionStart.SetFontFunctionLanguage(): Value = " + in_OptionData.Value);
   trace("OptionStart.SetFontFunctionLanguage(): FontList.length =  = " + in_OptionData.FontList.length);
   trace("OptionStart.SetFontFunctionLanguage(): FontList[ " + in_OptionData.Value + " ] = " + in_OptionData.FontList[in_OptionData.Value]);
   CommonSetFont(in_OptionData.OptionValueMC.text.text,in_OptionData.FontList[in_OptionData.Value]);
   return true;
}
function ButtonBack()
{
   trace("OptionStart.ButtonBack");
   var _loc1_;
   if(!bApplyAllCategory)
   {
      _loc1_ = 0;
      while(_loc1_ < OPTION_CATEGORY_NUM)
      {
         if(CategoryDataArray[_loc1_].bChange)
         {
            flash.external.ExternalInterface.call("ButtonBackStartPopUp");
            return undefined;
         }
         _loc1_ = _loc1_ + 1;
      }
   }
   if(bChange)
   {
      flash.external.ExternalInterface.call("ButtonBackStartPopUp");
   }
   else
   {
      gotoAndStop("close");
      play();
   }
}
function ButtonBackEndPopUp(in_bEnd)
{
   var _loc2_;
   var _loc1_;
   if(in_bEnd)
   {
      _loc2_ = 0;
      while(_loc2_ < OPTION_CATEGORY_NUM)
      {
         CategoryData = CategoryDataArray[_loc2_];
         _loc1_ = 0;
         while(_loc1_ < CategoryData.OptionDataArray.length)
         {
            OptionData = CategoryData.OptionDataArray[_loc1_];
            if(OptionData.BackFunction != undefined)
            {
               OptionData.BackFunction(OptionData);
            }
            _loc1_ = _loc1_ + 1;
         }
         _loc2_ = _loc2_ + 1;
      }
      gotoAndStop("close");
      play();
   }
   else
   {
      InputSetAllKeyDataEnable(true);
      ButtonRestart();
   }
   return !in_bEnd;
}
function ButtonApply()
{
   trace("OptionStart.ButtonApply");
   if(bApplyAllCategory)
   {
      ApplyAllCategoryData();
   }
   else
   {
      ApplyCategoryData(OptionCategoryIndex);
   }
   OptionDataSetChangeFlag(false);
   var _loc2_;
   var _loc1_;
   if(bChangeLanguage)
   {
      bChangeLanguage = false;
      TextData = flash.external.ExternalInterface.call("GetTextData");
      flash.external.ExternalInterface.call("SetText");
      CategoryDataArray[OptionCategoryIndex].SetText();
      _loc2_ = 0;
      while(_loc2_ < OPTION_CATEGORY_NUM)
      {
         CategoryData = CategoryDataArray[_loc2_];
         _loc1_ = 0;
         while(_loc1_ < CategoryData.OptionDataArray.length)
         {
            OptionData = CategoryData.OptionDataArray[_loc1_];
            if(OptionData.UpdateTextFunction != undefined)
            {
               OptionData.UpdateTextFunction(OptionData);
            }
            _loc1_ = _loc1_ + 1;
         }
         _loc2_ = _loc2_ + 1;
      }
   }
   ButtonRestart();
   return true;
}
function InputSetAllKeyEnable(in_flg)
{
   InputSetAllKeyDataEnableWithPushCheck(in_flg);
}
function ApplyAllCategoryData()
{
   var _loc2_ = {};
   var _loc3_ = ["Video","Audio","Other"];
   var _loc1_ = 0;
   while(_loc1_ < OPTION_CATEGORY_NUM)
   {
      _loc2_[_loc3_[_loc1_]] = CreateApplyCategoryData(_loc1_);
      _loc1_ = _loc1_ + 1;
   }
   flash.external.ExternalInterface.call("ApplyOptionData",_loc2_,OptionCategoryIndex,CategoryDataArray[OptionCategoryIndex].Index);
}
function ApplyCategoryData(in_CategoryIndex)
{
   var _loc1_ = CreateApplyCategoryData(in_CategoryIndex);
   switch(in_CategoryIndex)
   {
      case OPTION_CATEGORY_VIDEO:
         flash.external.ExternalInterface.call("ApplyNewVideoOptionData",_loc1_);
         break;
      case OPTION_CATEGORY_AUDIO:
         flash.external.ExternalInterface.call("ApplyNewAudioOptionData",_loc1_);
         break;
      case OPTION_CATEGORY_OTHER:
         flash.external.ExternalInterface.call("ApplyNewOtherOptionData",_loc1_);
      default:
         return;
   }
}
function CreateApplyCategoryData(in_CategoryIndex)
{
   trace("OptionStart.CreateApplyCategoryData( " + in_CategoryIndex + " )");
   var _loc5_ = CategoryDataArray[in_CategoryIndex];
   var _loc1_ = {};
   var _loc4_ = {};
   var _loc2_ = 0;
   while(_loc2_ < _loc5_.OptionDataArray.length)
   {
      _loc1_ = _loc5_.OptionDataArray[_loc2_];
      if(_loc1_.Name != undefined && _loc1_.GetApplyOptionDataFunction != undefined)
      {
         trace("OptionStart.CreateApplyCategoryData( " + in_CategoryIndex + " ): GetApplyOptionDataFunction( " + _loc1_.Name + " )");
         _loc4_[_loc1_.Name] = _loc1_.GetApplyOptionDataFunction(_loc1_);
         trace("OptionStart.CreateApplyCategoryData( " + in_CategoryIndex + " ): ApplyOptionData." + _loc1_.Name + " = " + _loc4_[_loc1_.Name]);
      }
      else if(_loc1_.Name != undefined)
      {
         trace("OptionStart.CreateApplyCategoryData( " + in_CategoryIndex + " ): OptionData.Name is undefined.");
      }
      else
      {
         trace("OptionStart.CreateApplyCategoryData( " + in_CategoryIndex + " ): GetApplyOptionDataFunction( " + _loc1_.Name + " ) is undefined.");
      }
      if(_loc1_.ApplyFunction != undefined)
      {
         _loc1_.ApplyFunction(_loc1_);
      }
      _loc2_ = _loc2_ + 1;
   }
   return _loc4_;
}
function ButtonInitialize()
{
   var _loc3_ = CategoryDataArray[OptionCategoryIndex];
   var _loc1_ = 0;
   var _loc2_;
   while(_loc1_ < _loc3_.OptionDataArray.length)
   {
      _loc2_ = _loc3_.OptionDataArray[_loc1_];
      _loc2_.SetDefaultFunction(_loc2_);
      _loc1_ = _loc1_ + 1;
   }
   OptionDataSetChangeFlag(true);
   InputSetAllKeyDataEnable(true);
   ButtonRestart();
   return true;
}
function ButtonCalibration()
{
   trace("OptionStart.ButtonCalibration");
   flash.external.ExternalInterface.call("StartCalibration");
}
function EndCalibration()
{
   trace("OptionStart.EndCalibration");
   window.etc.calibration.gotoAndPlay("over");
   InputSetAllKeyDataEnable(true);
   return true;
}
function ButtonRestart()
{
   var _loc1_ = CategoryDataArray[OptionCategoryIndex];
   var _loc2_ = _loc1_.OptionDataArray[_loc1_.Index];
   _loc2_.StartFunction(_loc2_);
}
function LeftFunctionSoundTest(in_OptionData)
{
   var _loc2_;
   var _loc1_;
   if(OptionDataCheck("OptionStart.LeftFunctionList",in_OptionData))
   {
      _loc2_ = in_OptionData.Value;
      _loc1_ = DecListValue(in_OptionData);
      if(_loc1_ == NaN)
      {
         trace("OptionStart.LeftFunctionList: NewValue is NaN.");
         return false;
      }
      if(_loc2_ != _loc1_)
      {
         flash.external.ExternalInterface.call("PlaySECursor");
      }
      return true;
   }
   return false;
}
function RightFunctionSoundTest(in_OptionData)
{
   var _loc2_;
   var _loc1_;
   if(OptionDataCheck("OptionStart.RightFunctionList",in_OptionData))
   {
      _loc2_ = in_OptionData.Value;
      _loc1_ = IncListValue(in_OptionData);
      if(_loc1_ == NaN)
      {
         trace("OptionStart.RightFunctionList: NewValue is NaN.");
         return false;
      }
      if(_loc2_ != _loc1_)
      {
         flash.external.ExternalInterface.call("PlaySECursor");
      }
      return true;
   }
   return false;
}
function SetDefaultFunctionSoundTest(in_OptionData)
{
   return true;
}
function CheckChangeFunctionSoundTest(in_OptionData)
{
   return false;
}
function StartFunctionSoundTest(in_OptionData)
{
   var _loc1_;
   if(PrevOptionData != undefined)
   {
      _loc1_ = 0;
      while(_loc1_ < SoundTestObjectArray.length)
      {
         if(PrevOptionData == SoundTestObjectArray[_loc1_])
         {
            break;
         }
         _loc1_ = _loc1_ + 1;
      }
      if(_loc1_ == SoundTestObjectArray.length)
      {
         flash.external.ExternalInterface.call("StartSoundTest");
      }
   }
   return StartFunctionSlider(in_OptionData);
}
function EndFunctionSoundTest(in_OptionData)
{
   var _loc1_;
   if(NextOptionData != undefined)
   {
      _loc1_ = 0;
      while(_loc1_ < SoundTestObjectArray.length)
      {
         if(NextOptionData == SoundTestObjectArray[_loc1_])
         {
            break;
         }
         _loc1_ = _loc1_ + 1;
      }
      if(_loc1_ == SoundTestObjectArray.length)
      {
         flash.external.ExternalInterface.call("EndSoundTest");
      }
   }
   return EndFunctionSlider(in_OptionData);
}
function DecideFunctionSoundTest(in_OptionData)
{
   flash.external.ExternalInterface.call("PlaySoundTest",in_OptionData.SoundTestType,in_OptionData.Value);
}
var BUTTON_TYPE_SYSTEM = 0;
var BUTTON_TYPE_OTHERS = 1;
var OptionButtonValue = 0;
var LanguageInitData;
var InitData = {};
var TextData = {};
var bChangeLanguage = false;
var SoundTestObjectArray = new Array();
trace("OptionStart");
Init();
