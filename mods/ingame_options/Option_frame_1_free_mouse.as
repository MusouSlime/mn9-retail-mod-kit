function Clamp(min, max, value)
{
   return Math.min(max,Math.max(min,value));
}
function CommonSetText(in_TextFieldPath, in_Text)
{
   var _loc3_ = in_TextFieldPath.split(".");
   var _loc4_ = this[_loc3_[0]];
   var _loc2_ = 1;
   while(_loc2_ < _loc3_.length)
   {
      _loc4_ = _loc4_[_loc3_[_loc2_]];
      _loc2_ = _loc2_ + 1;
   }
   _loc4_.text = in_Text;
}
function CommonSetTextLimitedLength(in_TextField, in_TestTextField, in_Text, in_LengthOverText)
{
   var _loc1_ = in_Text;
   if(in_TextField == undefined)
   {
      trace("CommonSetTextLimitedLength(): in_TextField is undefined.");
      return undefined;
   }
   if(in_TestTextField == undefined)
   {
      trace("CommonSetTextLimitedLength(): in_TestTextField is undefined.");
      return undefined;
   }
   if(in_TextField != in_TestTextField)
   {
      in_TestTextField._visible = false;
   }
   in_TestTextField.text = _loc1_;
   var _loc6_ = 5;
   var _loc4_;
   var _loc3_;
   if(_loc1_.length > 1 && in_TextField._width - _loc6_ <= in_TestTextField.textWidth)
   {
      _loc4_ = in_LengthOverText;
      if(_loc4_ == undefined)
      {
         _loc4_ = "";
      }
      _loc3_ = _loc4_.length;
      _loc1_ += _loc4_;
      while(true)
      {
         _loc1_ = _loc1_.slice(0,- (1 + _loc3_));
         _loc1_ += _loc4_;
         in_TestTextField.text = _loc1_;
         if(in_TextField._width - _loc6_ > in_TestTextField.textWidth)
         {
            break;
         }
         if(_loc1_.length <= 1 + _loc3_)
         {
            trace("CommonSetTextLimitedLength(): TestText.length is too short.");
            break;
         }
      }
   }
   in_TextField.text = _loc1_;
}
function CommonSetPlayerNameLimitedLength(in_TextField, in_TestTextField, in_Text)
{
   CommonSetTextLimitedLength(in_TextField,in_TestTextField,in_Text,"...");
}
function CommonSetFont(in_TextField, in_FontName)
{
   trace("Common.CommonSetFont( " + in_TextField + ", \"" + in_FontName + "\" )");
   var _loc1_ = in_TextField.getNewTextFormat();
   _loc1_.font = in_FontName;
   in_TextField.setTextFormat(_loc1_);
   in_TextField.setNewTextFormat(_loc1_);
   _loc1_ = in_TextField.getNewTextFormat();
   trace("Common.CommonSetFont( " + in_TextField + ", \"" + in_FontName + "\" ): TextFormat.font = " + _loc1_.font);
}
function CommonSetFontAllTextField(in_Object, in_FontName)
{
   trace("Common.CommonSetFontAllTextField( " + in_Object + ", \"" + in_FontName + "\" )");
   var _loc1_;
   for(var _loc4_ in in_Object)
   {
      _loc1_ = in_Object[_loc4_];
      if(_loc1_ == undefined)
      {
         trace("Common.CommonSetFontAllTextField( " + in_Object + ", \"" + in_FontName + "\" ): in_Object[ \"" + _loc4_ + "\" ] is undefined.");
      }
      else if(!(_loc1_ instanceof Object))
      {
         trace("Common.CommonSetFontAllTextField( " + in_Object + ", \"" + in_FontName + "\" ): in_Object[ \"" + _loc4_ + "\" ] is not object.");
      }
      else
      {
         if(_loc1_ instanceof TextField)
         {
            trace("Common.CommonSetFontAllTextField( " + in_Object + ", \"" + in_FontName + "\" ): in_Object[ \"" + _loc4_ + "\" ] is TextField.");
            CommonSetFont(TextField(_loc1_),in_FontName);
         }
         CommonSetFontAllTextField(_loc1_,in_FontName);
      }
   }
}
function TransformPos(in_X, in_Y, in_CanvasSizeX, in_CanvasSizeY)
{
   var _loc4_ = 1920;
   var _loc3_ = 1080;
   var _loc1_ = {};
   var _loc2_ = _loc4_ / _loc3_;
   var _loc5_ = in_CanvasSizeX / in_CanvasSizeY;
   _loc1_._x = in_X * _loc4_ / in_CanvasSizeX;
   _loc1_._y = in_Y * _loc3_ / in_CanvasSizeY;
   var _loc8_;
   var _loc6_;
   var _loc7_;
   if(_loc2_ != _loc5_)
   {
      if(_loc2_ > _loc5_)
      {
         _loc8_ = _loc2_ / _loc5_;
         _loc6_ = (_loc8_ - 1) / 2;
         _loc7_ = _loc6_ * _loc3_;
         _loc1_._y = in_Y * _loc4_ / in_CanvasSizeX - _loc7_;
      }
      else
      {
         _loc8_ = _loc5_ / _loc2_;
         _loc6_ = (_loc8_ - 1) / 2;
         _loc7_ = _loc6_ * _loc4_;
         _loc1_._x = in_X * _loc3_ / in_CanvasSizeY - _loc7_;
      }
   }
   return _loc1_;
}
function TextFieldSetColor(in_TextField, in_Start, in_End, in_Color)
{
   var _loc1_ = new TextFormat();
   _loc1_.color = in_Color;
   in_TextField.setTextFormat(in_Start,in_End,_loc1_);
}
function CommonSetNumMC(in_MC, in_DigitMCName, in_DigitStart, in_DigitNum, in_bFillZero, in_Num)
{
   var _loc3_ = Math.floor(in_Num);
   var _loc4_ = in_DigitStart;
   var _loc6_ = _loc4_ + in_DigitNum;
   var _loc2_;
   var _loc5_;
   var _loc1_ = _loc4_;
   while(_loc1_ < _loc6_)
   {
      _loc2_ = in_MC[in_DigitMCName + _loc1_];
      if(_loc2_ != undefined)
      {
         _loc5_ = _loc3_ % 10;
         if(_loc3_ == 0 && _loc1_ > _loc4_ && !in_bFillZero)
         {
            _loc2_._visible = false;
         }
         else
         {
            _loc2_._visible = true;
            _loc2_.gotoAndPlay("num" + _loc5_);
         }
         _loc3_ = Math.floor(_loc3_ / 10);
      }
      _loc1_ = _loc1_ + 1;
   }
}
function InputInit()
{
   InputInitData = flash.external.ExternalInterface.call("GetInputInitData");
   trace("Input.InputInit: InputInitData = " + InputInitData);
   trace("Input.InputInit: ClassName = " + InputInitData.ClassName);
   InputKeyRepeatIntervalFirst = InputInitData.KeyRepeatIntervalFirst;
   InputKeyRepeatInterval = InputInitData.KeyRepeatInterval;
   InputKeyRapidInterval = InputInitData.KeyRapidInterval;
   InputDeleteAllKeyData();
}
function InputAddKeyData(in_WorkName, in_TriggerFunction, in_RepeatFunction, in_ReleaseFunction)
{
   var _loc3_ = {};
   var _loc2_ = InputInitData.KeyDataArray;
   _loc3_.WorkName = in_WorkName;
   var _loc5_;
   var _loc4_ = 0;
   while(_loc4_ < _loc2_.length)
   {
      if(_loc2_[_loc4_].WorkName == in_WorkName)
      {
         _loc5_ = _loc2_[_loc4_];
         _loc3_.InitKeyDataIndex = _loc4_;
         break;
      }
      _loc4_ = _loc4_ + 1;
   }
   var _loc1_;
   if(_loc5_ == undefined)
   {
      trace(InputInitData.ClassName + ".Input.InputAddKeyData( " + in_WorkName + " ): InitKeyData is not exist.");
   }
   else
   {
      _loc3_.KeyCodeArray = _loc5_.KeyArray;
      _loc3_.CancelOutKeyDataIndexArray = new Array();
      _loc4_ = 0;
      while(_loc4_ < _loc5_.CancelOutWorkNameArray.length)
      {
         _loc1_ = 0;
         while(_loc1_ < _loc2_.length)
         {
            if(_loc2_[_loc1_].WorkName != in_WorkName && _loc2_[_loc1_].WorkName == _loc5_.CancelOutWorkNameArray[_loc4_])
            {
               _loc3_.CancelOutKeyDataIndexArray.push(_loc1_);
               break;
            }
            _loc1_ = _loc1_ + 1;
         }
         _loc4_ = _loc4_ + 1;
      }
   }
   _loc3_.KeyCodeTrigger = INPUT_KEY_CODE_INVALID;
   _loc3_.KeyCodeDisablePush = INPUT_KEY_CODE_INVALID;
   _loc3_.bEnable = true;
   _loc3_.bPush = false;
   _loc3_.bPushFirst = false;
   _loc3_.bRepeatEnable = true;
   _loc3_.RepeatInterval = InputKeyRepeatInterval;
   _loc3_.RepeatIntervalFirst = InputKeyRepeatIntervalFirst;
   _loc3_.RapidInterval = InputKeyRapidInterval;
   _loc3_.Count = 0;
   _loc3_.CountTime = _loc3_.RapidInterval;
   _loc3_.TriggerFunction = in_TriggerFunction;
   _loc3_.RepeatFunction = in_RepeatFunction;
   _loc3_.ReleaseFunction = in_ReleaseFunction;
   InputKeyDataArray.push(_loc3_);
   return _loc3_;
}
function InputAddKeyDataRepeatDisable(in_WorkName, in_TriggerFunction, in_ReleaseFunction)
{
   var _loc1_ = InputAddKeyData(in_WorkName,in_TriggerFunction,function()
   {
   }
   ,in_ReleaseFunction);
   _loc1_.bRepeatEnable = false;
   return _loc1_;
}
function InputUpdate()
{
   if(!InputEnableFlag)
   {
      return undefined;
   }
   var _loc3_ = flash.external.ExternalInterface.call("GetAllKeyPushFlag");
   var _loc2_;
   var _loc1_ = 0;
   while(_loc1_ < InputKeyDataArray.length)
   {
      _loc2_ = InputKeyDataArray[_loc1_];
      InputUpdateKeyData(_loc2_,_loc3_);
      _loc1_ = _loc1_ + 1;
   }
}
function InputUpdateKeyData(in_KeyData, in_AllKeyPushFlagArray)
{
   if(in_KeyData.Count < in_KeyData.CountTime)
   {
      in_KeyData.Count = in_KeyData.Count + 1;
   }
   var _loc5_ = false;
   var _loc6_ = {};
   var _loc4_ = 0;
   var _loc2_;
   while(_loc4_ < in_KeyData.CancelOutKeyDataIndexArray.length)
   {
      _loc6_ = in_AllKeyPushFlagArray[in_KeyData.CancelOutKeyDataIndexArray[_loc4_]];
      _loc2_ = 0;
      while(_loc2_ < _loc6_.length)
      {
         if(_loc6_[_loc2_])
         {
            _loc5_ = true;
            break;
         }
         _loc2_ = _loc2_ + 1;
      }
      if(_loc5_)
      {
         break;
      }
      _loc4_ = _loc4_ + 1;
   }
   var _loc8_ = in_AllKeyPushFlagArray[in_KeyData.InitKeyDataIndex];
   var _loc9_ = in_KeyData.KeyCodeTrigger;
   var _loc3_;
   _loc4_ = 0;
   var _loc7_;
   while(_loc4_ < in_KeyData.KeyCodeArray.length)
   {
      _loc3_ = in_KeyData.KeyCodeArray[_loc4_];
      _loc7_ = _loc8_[_loc4_];
      if(_loc9_ == _loc3_)
      {
         if(_loc7_)
         {
            if(in_KeyData.bPush)
            {
               if(in_KeyData.bRepeatEnable)
               {
                  if(_loc5_)
                  {
                     break;
                  }
                  InputKeyDataRepeat(in_KeyData,_loc3_);
                  break;
               }
            }
         }
         else if(in_KeyData.bPush)
         {
            InputKeyDataRelease(in_KeyData,_loc3_);
            break;
         }
      }
      else if(in_KeyData.KeyCodeDisablePush == _loc3_)
      {
         if(!_loc7_)
         {
            if(in_KeyData.bPush)
            {
               in_KeyData.KeyCodeDisablePush = INPUT_KEY_CODE_INVALID;
               in_KeyData.bPush = false;
               in_KeyData.bPushFirst = false;
               in_KeyData.Count = 0;
               in_KeyData.CountTime = in_KeyData.RapidInterval;
            }
         }
      }
      else if(_loc7_)
      {
         if(_loc5_)
         {
            break;
         }
         InputKeyDataTrigger(in_KeyData,_loc3_);
         break;
      }
      _loc4_ = _loc4_ + 1;
   }
}
function InputKeyDataTrigger(in_KeyData, in_KeyCode)
{
   if(!in_KeyData.bEnable)
   {
      in_KeyData.bPush = true;
      in_KeyData.KeyCodeTrigger = INPUT_KEY_CODE_INVALID;
      in_KeyData.KeyCodeDisablePush = in_KeyCode;
      return undefined;
   }
   in_KeyData.KeyCodeTrigger = in_KeyCode;
   in_KeyData.bPush = true;
   in_KeyData.bPushFirst = true;
   in_KeyData.Count = 0;
   in_KeyData.CountTime = in_KeyData.RepeatIntervalFirst;
   in_KeyData.TriggerFunction(in_KeyData,in_KeyCode);
}
function InputKeyDataRepeat(in_KeyData, in_KeyCode)
{
   var _loc2_ = in_KeyData.RepeatInterval;
   if(in_KeyData.Count >= in_KeyData.CountTime)
   {
      in_KeyData.bPushFirst = false;
      in_KeyData.Count = 0;
      in_KeyData.CountTime = in_KeyData.RepeatInterval;
      in_KeyData.RepeatFunction(in_KeyData,in_KeyCode);
   }
}
function InputKeyDataRelease(in_KeyData, in_KeyCode)
{
   in_KeyData.ReleaseFunction(in_KeyData,in_KeyCode);
   in_KeyData.KeyCodeTrigger = INPUT_KEY_CODE_INVALID;
   in_KeyData.bPush = false;
   in_KeyData.bPushFirst = false;
   in_KeyData.Count = 0;
   in_KeyData.CountTime = in_KeyData.RapidInterval;
}
function InputGetKeyDataByWorkName(in_WorkName)
{
   var _loc2_;
   var _loc1_ = 0;
   while(_loc1_ < InputKeyDataArray.length)
   {
      _loc2_ = InputKeyDataArray[_loc1_];
      if(_loc2_.WorkName == in_WorkName)
      {
         return _loc2_;
      }
      _loc1_ = _loc1_ + 1;
   }
   return undefined;
}
function InputSearchKeyDataByWorkName(in_WorkName)
{
   var _loc2_;
   var _loc1_ = 0;
   while(_loc1_ < InputKeyDataArray.length)
   {
      _loc2_ = InputKeyDataArray[_loc1_];
      if(_loc2_.WorkName == in_WorkName)
      {
         return _loc1_;
      }
      _loc1_ = _loc1_ + 1;
   }
   return INPUT_KEY_DATA_INDEX_INVALID;
}
function InputSearchKeyDataByOneKeyCode(in_KeyCode)
{
   var _loc3_;
   var _loc2_ = 0;
   var _loc1_;
   while(_loc2_ < InputKeyDataArray.length)
   {
      _loc3_ = InputKeyDataArray[_loc2_];
      _loc1_ = 0;
      while(_loc1_ < _loc3_.KeyCodeArray.length)
      {
         if(_loc3_.KeyCodeArray[_loc1_] == in_KeyCode)
         {
            return _loc2_;
         }
         _loc1_ = _loc1_ + 1;
      }
      _loc2_ = _loc2_ + 1;
   }
   return INPUT_KEY_DATA_INDEX_INVALID;
}
function InputSearchKeyDataByAllKeyCode(in_KeyCodeArray)
{
   var _loc2_;
   var _loc4_;
   var _loc3_ = 0;
   var _loc1_;
   while(_loc3_ < InputKeyDataArray.length)
   {
      _loc2_ = InputKeyDataArray[_loc3_];
      if(_loc2_.KeyCodeArray.length == in_KeyCodeArray.length)
      {
         _loc4_ = true;
         _loc1_ = 0;
         while(_loc1_ < _loc2_.KeyCodeArray.length)
         {
            if(_loc2_.KeyCodeArray[_loc1_] != in_KeyCodeArray[_loc1_])
            {
               _loc4_ = false;
               break;
            }
            _loc1_ = _loc1_ + 1;
         }
         if(_loc4_)
         {
            return _loc3_;
         }
      }
      _loc3_ = _loc3_ + 1;
   }
   return INPUT_KEY_DATA_INDEX_INVALID;
}
function InputSetInputEnable(in_bInputEnable)
{
   var _loc1_ = InputEnableFlag;
   InputEnableFlag = in_bInputEnable;
   if(InputEnableFlag != _loc1_)
   {
      if(InputEnableFlag)
      {
         InputAllKeyDataResetKeyState();
      }
   }
}
function InputAllKeyDataResetKeyState()
{
   var _loc4_ = flash.external.ExternalInterface.call("GetAllKeyPushFlag");
   var _loc2_;
   var _loc3_;
   var _loc1_ = 0;
   while(_loc1_ < InputKeyDataArray.length)
   {
      _loc2_ = InputKeyDataArray[_loc1_];
      _loc3_ = _loc4_[_loc2_.InitKeyDataIndex];
      InputResetKeyState(_loc2_,_loc3_);
      _loc1_ = _loc1_ + 1;
   }
}
function InputResetKeyState(in_KeyData, in_KeyPushFlagArray)
{
   if(in_KeyData.bRepeatEnable)
   {
      in_KeyData.KeyCodeTrigger = INPUT_KEY_CODE_INVALID;
      in_KeyData.KeyCodeDisablePush = INPUT_KEY_CODE_INVALID;
      in_KeyData.bPush = false;
      in_KeyData.bPushFirst = false;
      in_KeyData.Count = 0;
      in_KeyData.CountTime = in_KeyData.RapidInterval;
      return undefined;
   }
   var _loc2_ = 0;
   while(_loc2_ < in_KeyData.KeyCodeArray.length)
   {
      if(in_KeyPushFlagArray[_loc2_])
      {
         in_KeyData.KeyCodeTrigger = INPUT_KEY_CODE_INVALID;
         in_KeyData.KeyCodeDisablePush = in_KeyPushFlagArray[_loc2_];
         in_KeyData.bPush = true;
         return undefined;
      }
      _loc2_ = _loc2_ + 1;
   }
   in_KeyData.KeyCodeTrigger = INPUT_KEY_CODE_INVALID;
   in_KeyData.KeyCodeDisablePush = INPUT_KEY_CODE_INVALID;
   in_KeyData.bPush = false;
   in_KeyData.bPushFirst = false;
   in_KeyData.Count = 0;
   in_KeyData.CountTime = in_KeyData.RapidInterval;
}
function InputResetKeyStateByWorkName(in_WorkName, in_KeyPushFlagArray)
{
   var _loc1_ = InputSearchKeyDataByWorkName(in_WorkName);
   if(_loc1_ != INPUT_KEY_DATA_INDEX_INVALID)
   {
      InputResetKeyState(InputKeyDataArray[_loc1_],in_KeyPushFlagArray);
   }
   else
   {
      trace(InputInitData.ClassName + ".InputResetKeyStateByWorkName: Index is invalid.");
   }
}
function InputSetKeyDataEnable(in_KeyData, in_bEnable)
{
   in_KeyData.bEnable = in_bEnable;
   in_KeyData.Count = in_KeyData.InputKeyRapidInterval;
   if(in_KeyData.bRepeatEnable)
   {
      in_KeyData.KeyCodeTrigger = INPUT_KEY_CODE_INVALID;
      in_KeyData.KeyCodeDisablePush = INPUT_KEY_CODE_INVALID;
      in_KeyData.bPush = false;
      in_KeyData.bPushFirst = false;
      in_KeyData.Count = 0;
      in_KeyData.CountTime = in_KeyData.RapidInterval;
   }
}
function InputSetAllKeyDataEnable(in_bEnable)
{
   var _loc1_ = 0;
   while(_loc1_ < InputKeyDataArray.length)
   {
      InputSetKeyDataEnable(InputKeyDataArray[_loc1_],in_bEnable);
      _loc1_ = _loc1_ + 1;
   }
}
function InputSetKeyDataEnableWithPushCheck(in_KeyData, in_bEnable, in_KeyPushFlagArray)
{
   in_KeyData.bEnable = in_bEnable;
   in_KeyData.Count = in_KeyData.InputKeyRapidInterval;
   var _loc1_;
   if(in_KeyData.bRepeatEnable)
   {
      in_KeyData.KeyCodeTrigger = INPUT_KEY_CODE_INVALID;
      in_KeyData.KeyCodeDisablePush = INPUT_KEY_CODE_INVALID;
      in_KeyData.bPush = false;
      in_KeyData.bPushFirst = false;
      in_KeyData.Count = 0;
      in_KeyData.CountTime = in_KeyData.RapidInterval;
   }
   else
   {
      _loc1_ = 0;
      while(_loc1_ < in_KeyData.KeyCodeArray.length)
      {
         KeyCode = in_KeyData.KeyCodeArray[_loc1_];
         if(in_KeyPushFlagArray[_loc1_])
         {
            in_KeyData.KeyCodeTrigger = KeyCode;
            in_KeyData.bPush = true;
         }
         _loc1_ = _loc1_ + 1;
      }
   }
}
function InputSetAllKeyDataEnableWithPushCheck(in_bEnable)
{
   var _loc4_ = flash.external.ExternalInterface.call("GetAllKeyPushFlag");
   var _loc2_;
   var _loc3_;
   var _loc1_ = 0;
   while(_loc1_ < InputKeyDataArray.length)
   {
      _loc2_ = InputKeyDataArray[_loc1_];
      _loc3_ = _loc4_[_loc2_.InitKeyDataIndex];
      InputSetKeyDataEnableWithPushCheck(_loc2_,in_bEnable,_loc3_);
      _loc1_ = _loc1_ + 1;
   }
}
function InputSetKeyDataEnableByIndex(in_Index, in_bEnable)
{
   if(in_Index >= 0 && in_Index < InputKeyDataArray.length)
   {
      InputSetKeyDataEnable(InputKeyDataArray[in_Index],in_bEnable);
   }
}
function InputSetKeyDataEnableByWorkName(in_WorkName, in_bEnable)
{
   var _loc1_ = InputSearchKeyDataByWorkName(in_WorkName);
   if(_loc1_ != INPUT_KEY_DATA_INDEX_INVALID)
   {
      InputSetKeyDataEnable(InputKeyDataArray[_loc1_],in_bEnable);
   }
   else
   {
      trace(InputInitData.ClassName + ".InputSetKeyDataEnableByWorkName: Index is invalid.");
   }
}
function InputSetKeyDataEnableByOneKeyCode(in_KeyCode, in_bEnable)
{
   var _loc1_ = InputSearchKeyDataByOneKeyCode(in_KeyCode);
   if(_loc1_ != INPUT_KEY_DATA_INDEX_INVALID)
   {
      InputSetKeyDataEnable(InputKeyDataArray[_loc1_],in_bEnable);
   }
   else
   {
      trace(InputInitData.ClassName + ".InputSetKeyDataEnableByOneKeyCode: Index is invalid.");
   }
}
function InputSetKeyDataEnableByAllKeyCode(in_KeyCodeArray, in_bEnable)
{
   var _loc1_ = InputSearchKeyDataByAllKeyCode(in_KeyCodeArray);
   if(_loc1_ != INPUT_KEY_DATA_INDEX_INVALID)
   {
      InputSetKeyDataEnable(InputKeyDataArray[_loc1_],in_bEnable);
   }
   else
   {
      trace(InputInitData.ClassName + ".InputSetKeyDataEnableByAllKeyCode: Index is invalid.");
   }
}
function InputDeleteAllKeyData()
{
   InputKeyDataArray.length = 0;
}
function InputDeleteKeyDataByIndex(in_Index)
{
   InputKeyDataArray.splice(in_Index,1);
}
function InputDeleteKeyDataByWorkName(in_WorkName)
{
   var _loc1_ = InputSearchKeyDataByWorkName(in_WorkName);
   if(_loc1_ != INPUT_KEY_DATA_INDEX_INVALID)
   {
      InputDeleteKeyDataByIndex(_loc1_);
   }
   else
   {
      trace(InputInitData.ClassName + ".InputDeleteKeyDataByWorkName: Index is invalid.");
   }
}
function InputDeleteKeyDataByOneKeyCode(in_KeyCode)
{
   var _loc1_ = InputSearchKeyDataByOneKeyCode(in_KeyCode);
   if(_loc1_ != INPUT_KEY_DATA_INDEX_INVALID)
   {
      InputDeleteKeyDataByIndex(_loc1_);
   }
   else
   {
      trace(InputInitData.ClassName + ".InputDeleteKeyDataByOneKeyCode: Index is invalid.");
   }
}
function InputDeleteKeyDataByAllKeyCode(in_KeyCodeArray)
{
   var _loc1_ = InputSearchKeyDataByAllKeyCode(in_KeyCodeArray);
   if(_loc1_ != INPUT_KEY_DATA_INDEX_INVALID)
   {
      InputDeleteKeyDataByIndex(_loc1_);
   }
   else
   {
      trace(InputInitData.ClassName + ".InputDeleteKeyDataByAllKeyCode: Index is invalid.");
   }
}
function InputIsPush(in_KeyData)
{
   if(in_KeyData.bEnable)
   {
      return in_KeyData.bPush;
   }
   return false;
}
function InputIsPushByIndex(in_Index)
{
   return InputIsPush(InputKeyDataArray[in_Index]);
}
function InputIsPushByWorkName(in_WorkName)
{
   var _loc1_ = InputSearchKeyDataByWorkName(in_WorkName);
   if(_loc1_ != INPUT_KEY_DATA_INDEX_INVALID)
   {
      return InputIsPushByIndex(_loc1_);
   }
   trace(InputInitData.ClassName + ".InputIsPushByWorkName: Index is invalid.");
   return false;
}
function InputIsPushByOneKeyCode(in_KeyCode)
{
   var _loc1_ = InputSearchKeyDataByOneKeyCode(in_KeyCode);
   if(_loc1_ != INPUT_KEY_DATA_INDEX_INVALID)
   {
      return InputIsPushByIndex(_loc1_);
   }
   trace(InputInitData.ClassName + ".InputIsPushByOneKeyCode: Index is invalid.");
   return false;
}
function InputIsPushByAllKeyCode(in_KeyCodeArray)
{
   var _loc1_ = InputSearchKeyDataByAllKeyCode(in_KeyCodeArray);
   if(_loc1_ != INPUT_KEY_DATA_INDEX_INVALID)
   {
      return InputIsPushByIndex(_loc1_);
   }
   trace(InputInitData.ClassName + ".InputDeleteKeyDataByAllKeyCode: Index is invalid.");
   return false;
}
function InitKeyData()
{
   InputInit();
   InputAddKeyDataRepeatDisable("Decide",Decide,function()
   {
   }
   );
   InputAddKeyDataRepeatDisable("Cancel",Cancel,function()
   {
   }
   );
   InputAddKeyData("Up",Up,Up,function()
   {
   }
   );
   InputAddKeyData("Down",Down,Down,function()
   {
   }
   );
   InputAddKeyData("Left",Left,Left,function()
   {
   }
   );
   InputAddKeyData("Right",Right,Right,function()
   {
   }
   );
   InputAddKeyDataRepeatDisable("TabLeft",TabLeft,function()
   {
   }
   );
   InputAddKeyDataRepeatDisable("TabRight",TabRight,function()
   {
   }
   );
   InputAddKeyDataRepeatDisable("Apply",PushApply,function()
   {
   }
   );
   InputAddKeyDataRepeatDisable("Default",PushInitialize,function()
   {
   }
   );
   InputAddKeyDataRepeatDisable("Wide",SubButton,function()
   {
   }
   );
   onEnterFrame = InputUpdate;
   InputSetAllKeyDataEnable(false);
}
function ReinitInput()
{
   trace("OptionControle.ReinitInput");
   InputSetAllKeyDataEnable(false);
   onEnterFrame = function()
   {
   };
   InitKeyData();
}
function Decide()
{
   var _loc2_ = CategoryDataArray[OptionCategoryIndex];
   var _loc1_ = _loc2_.OptionDataArray[_loc2_.Index];
   if(_loc1_.DecideFunction != undefined)
   {
      _loc1_.DecideFunction(_loc1_);
   }
}
function Cancel()
{
   InputSetAllKeyDataEnable(false);
   flash.external.ExternalInterface.call("PlaySECancel");
   ButtonBack();
}
function End()
{
   gotoAndStop("close");
   play();
}
function Back()
{
}
function Up()
{
   var _loc1_ = CategoryDataArray[OptionCategoryIndex];
   var _loc2_ = {};
   var _loc3_ = _loc1_.Index;
   do
   {
      _loc1_.Index = _loc1_.Index - 1;
      if(_loc1_.Index < 0)
      {
         _loc1_.Index += _loc1_.OptionDataArray.length;
      }
      _loc2_ = _loc1_.OptionDataArray[_loc1_.Index];
   }
   while(!_loc2_.bEnable);
   trace("Up, Prev = " + _loc3_ + ", New = " + _loc1_.Index);
   var _loc4_;
   if(_loc3_ != _loc1_.Index)
   {
      _loc4_ = _loc1_.OptionDataArray[_loc3_];
      NextOptionData = _loc2_;
      _loc4_.EndFunction(_loc4_);
      PrevOptionData = _loc4_;
      _loc2_.StartFunction(_loc2_);
      flash.external.ExternalInterface.call("PlaySECursor");
   }
}
function Down()
{
   var _loc1_ = CategoryDataArray[OptionCategoryIndex];
   var _loc2_ = {};
   var _loc3_ = _loc1_.Index;
   do
   {
      _loc1_.Index = _loc1_.Index + 1;
      if(_loc1_.Index >= _loc1_.OptionDataArray.length)
      {
         _loc1_.Index -= _loc1_.OptionDataArray.length;
      }
      _loc2_ = _loc1_.OptionDataArray[_loc1_.Index];
   }
   while(!_loc2_.bEnable);
   trace("Down, Prev = " + _loc3_ + ", New = " + _loc1_.Index);
   var _loc4_;
   if(_loc3_ != _loc1_.Index)
   {
      _loc4_ = _loc1_.OptionDataArray[_loc3_];
      NextOptionData = _loc2_;
      _loc4_.EndFunction(_loc4_);
      PrevOptionData = _loc4_;
      _loc2_.StartFunction(_loc2_);
      flash.external.ExternalInterface.call("PlaySECursor");
   }
}
function Left()
{
   trace("OptionControl.Left");
   var _loc1_ = CategoryDataArray[OptionCategoryIndex];
   var _loc2_ = _loc1_.OptionDataArray[_loc1_.Index];
   _loc2_.LeftFunction(_loc2_);
}
function Right()
{
   trace("OptionControl.Right");
   var _loc1_ = CategoryDataArray[OptionCategoryIndex];
   var _loc2_ = _loc1_.OptionDataArray[_loc1_.Index];
   _loc2_.RightFunction(_loc2_);
}
function SubButton()
{
   var _loc2_ = CategoryDataArray[OptionCategoryIndex];
   var _loc1_ = _loc2_.OptionDataArray[_loc2_.Index];
   if(_loc1_.SubButtonFunction != undefined)
   {
      _loc1_.SubButtonFunction(_loc1_);
   }
}
function TabLeft()
{
   trace("OptionControl.TabLeft");
   DecCategoryIndex();
   window.LR.gotoAndPlay("inputL");
   flash.external.ExternalInterface.call("PlaySECursor");
}
function TabRight()
{
   trace("OptionControl.TabRight");
   IncCategoryIndex();
   window.LR.gotoAndPlay("inputR");
   flash.external.ExternalInterface.call("PlaySECursor");
}
function PushInitialize()
{
   trace("OptionControl.PushInitialize");
   var _loc1_ = CategoryDataArray[OptionCategoryIndex];
   if(_loc1_.InitData.bInitializeEnable)
   {
      ButtonInitialize();
   }
}
function PushApply()
{
   trace("OptionControl.PushApply");
   var _loc2_;
   var _loc1_;
   if(bApplyAllCategory)
   {
      _loc1_ = 0;
      while(_loc1_ < OPTION_CATEGORY_NUM)
      {
         _loc2_ = CategoryDataArray[_loc1_];
         if(_loc2_.bChange)
         {
            InputSetAllKeyDataEnable(false);
            ButtonApply();
            return undefined;
         }
         _loc1_ = _loc1_ + 1;
      }
   }
   else
   {
      _loc2_ = CategoryDataArray[OptionCategoryIndex];
      if(_loc2_.bChange)
      {
         InputSetAllKeyDataEnable(false);
         ButtonApply();
         return undefined;
      }
   }
}
function StartUI()
{
   trace("OptionControl.StartUI");
   bStartUI = true;
   if(bMain)
   {
      InputSetAllKeyDataEnable(true);
   }
}
function SetCategoryIndex(Index)
{
   trace("OptionControl.SetCategoryIndex( " + Index + " )");
   var _loc3_ = CategoryDataArray[OptionCategoryIndex];
   var _loc2_ = _loc3_.OptionDataArray[_loc3_.Index];
   if(NextOptionData == _loc2_ && _loc2_.Type != OPTION_TYPE_CATEGORY)
   {
      _loc2_.EndFunction(_loc2_);
   }
   OptionCategoryIndex = Index;
   var _loc1_ = CategoryDataArray[OptionCategoryIndex];
   if(InitData != undefined && InitData.StartOptionIndex != undefined && InitData.StartOptionIndex >= 0)
   {
      _loc1_.Index = InitData.StartOptionIndex;
      InitData.StartOptionIndex = -1;
   }
   else if(_loc3_.Index == 0)
   {
      _loc1_.Index = 0;
   }
   else
   {
      _loc1_.Index = 1;
   }
   window.defaultSetting._visible = _loc1_.InitData.bInitializeEnable;
   bInitCategoryData = false;
   NextOptionData = undefined;
   PrevOptionData = undefined;
   window.gotoAndPlay("normal" + (OptionCategoryIndex + 1));
}
function IncCategoryIndex()
{
   var _loc1_ = OptionCategoryIndex + 1;
   if(_loc1_ >= OPTION_CATEGORY_NUM)
   {
      _loc1_ -= OPTION_CATEGORY_NUM;
   }
   SetCategoryIndex(_loc1_);
}
function DecCategoryIndex()
{
   var _loc1_ = OptionCategoryIndex - 1;
   if(_loc1_ < 0)
   {
      _loc1_ += OPTION_CATEGORY_NUM;
   }
   SetCategoryIndex(_loc1_);
}
function InitOptionData(in_Category, in_ParentMC)
{
   trace("OptionContorol.InitOptionData( " + in_Category + ", " + in_ParentMC + " )");
   var _loc8_ = CategoryDataArray[in_Category];
   if(_loc8_ == undefined)
   {
      trace("OptionContorol.InitOptionData: OptionCategoryData is undefined.");
      return false;
   }
   if(in_ParentMC == undefined)
   {
      trace("OptionContorol.InitOptionData: in_ParentMC is undefined.");
      return false;
   }
   if(_loc8_.bChange)
   {
      _loc8_.SystemButtonData.ButtonDataArray[1].bEnable = true;
   }
   else
   {
      _loc8_.SystemButtonData.ButtonDataArray[1].bEnable = false;
   }
   var _loc1_ = {};
   var _loc15_ = NaN;
   var _loc7_ = NaN;
   var _loc6_ = 0;
   var _loc5_ = 0;
   var _loc2_;
   var _loc3_;
   var _loc9_;
   var _loc12_;
   var _loc10_;
   var _loc11_;
   var _loc13_;
   var _loc14_;
   while(_loc5_ < _loc8_.OptionDataArray.length)
   {
      _loc1_ = _loc8_.OptionDataArray[_loc5_];
      if(_loc1_.Type != OPTION_TYPE_CATEGORY)
      {
         if(_loc1_.Type == OPTION_TYPE_BUTTON)
         {
            _loc2_ = {};
            _loc3_ = 0;
            while(_loc3_ < _loc1_.ButtonDataArray.length)
            {
               _loc2_ = _loc1_.ButtonDataArray[_loc3_];
               if(_loc2_.ButtonMC == undefined)
               {
                  _loc2_.ButtonMC = in_ParentMC[_loc2_.MCName];
               }
               _loc3_ = _loc3_ + 1;
            }
         }
         else
         {
            if(in_ParentMC[_loc1_.MCName] == undefined)
            {
               _loc9_ = 244;
               _loc12_ = 0;
               _loc10_ = 72;
               _loc1_.OptionNameMC = in_ParentMC.attachMovie("buttonLine",_loc1_.MCName,_loc5_ * 2);
               _loc1_.OptionNameMC._x = _loc9_ + _loc12_ * _loc6_;
               _loc1_.OptionNameMC._y = _loc15_ + _loc10_ * _loc6_;
            }
            else
            {
               _loc1_.OptionNameMC = in_ParentMC[_loc1_.MCName];
               if(_loc7_ == NaN)
               {
                  _loc15_ = _loc1_.OptionNameMC._y;
               }
            }
            if(in_ParentMC[_loc1_.MCName + "Setting"] == undefined)
            {
               _loc11_ = 756;
               _loc13_ = 0;
               _loc14_ = 72;
               _loc1_.OptionValueMC = in_ParentMC.attachMovie(SettingMCNameArray[_loc1_.Type],_loc1_.MCName,_loc5_ * 2 + 1);
               _loc1_.OptionValueMC._x = _loc11_ + _loc13_ * _loc6_;
               _loc1_.OptionValueMC._y = _loc7_ + _loc14_ * _loc6_;
            }
            else
            {
               _loc1_.OptionValueMC = in_ParentMC[_loc1_.MCName + "Setting"];
               if(_loc7_ == NaN)
               {
                  _loc7_ = _loc1_.OptionValueMC._y;
               }
            }
            _loc6_ = _loc6_ + 1;
         }
      }
      _loc1_.InitFunction(_loc1_);
      if(_loc1_.MCName == "mn9mouse" && _loc1_.OptionNameMC != undefined)
      {
         _loc1_.OptionNameMC.text.text.text = "Cursor Hold";
      }
      _loc5_ = _loc5_ + 1;
   }
   _loc1_ = _loc8_.OptionDataArray[_loc8_.Index];
   _loc1_.StartFunction(_loc1_);
   return true;
}
function OptionDataCheck(in_HeaderString, in_OptionData)
{
   if(in_OptionData == undefined)
   {
      trace(in_HeaderString + ": in_OptionData is undefined.");
      return false;
   }
   if(in_OptionData.OptionNameMC == undefined)
   {
      trace(in_HeaderString + ": OptionNameMC is undefined. MCName = " + in_OptionData.MCName);
      return false;
   }
   if(in_OptionData.OptionValueMC == undefined)
   {
      trace(in_HeaderString + ": OptionValueMC is undefined. MCName = " + in_OptionData.MCName);
      return false;
   }
   return true;
}
function OptionDataSetChangeFlag(in_bChange)
{
   var _loc2_;
   var _loc3_;
   var _loc1_;
   if(bApplyAllCategory)
   {
      _loc1_ = 0;
      while(_loc1_ < OPTION_CATEGORY_NUM)
      {
         _loc2_ = CategoryDataArray[_loc1_];
         _loc3_ = _loc2_.SystemButtonData.ButtonDataArray[1];
         _loc2_.bChange = in_bChange;
         _loc3_.bEnable = in_bChange;
         _loc1_ = _loc1_ + 1;
      }
      _loc2_ = CategoryDataArray[OptionCategoryIndex];
      _loc3_ = _loc2_.SystemButtonData.ButtonDataArray[1];
   }
   else
   {
      _loc2_ = CategoryDataArray[OptionCategoryIndex];
      _loc3_ = _loc2_.SystemButtonData.ButtonDataArray[1];
      _loc2_.bChange = in_bChange;
      _loc3_.bEnable = in_bChange;
   }
   bChange = in_bChange;
   if(bChange)
   {
      _loc3_.ButtonMC.gotoAndPlay("up");
   }
   else
   {
      _loc3_.ButtonMC.gotoAndPlay("disabled");
   }
}
function GetCurrentValueInt(in_OptionName)
{
   var _loc3_;
   var _loc1_;
   var _loc4_ = 0;
   var _loc2_;
   while(_loc4_ < OPTION_CATEGORY_NUM)
   {
      _loc3_ = CategoryDataArray[_loc4_];
      _loc2_ = 0;
      while(_loc2_ < _loc3_.OptionDataArray.length)
      {
         _loc1_ = _loc3_.OptionDataArray[_loc2_];
         if(_loc1_.Name == in_OptionName)
         {
            if(_loc1_.Value != undefined)
            {
               return _loc1_.Value;
            }
            if(_loc1_.bOn != undefined)
            {
               return !_loc1_.bOn ? 0 : 1;
            }
         }
         _loc2_ = _loc2_ + 1;
      }
      _loc4_ = _loc4_ + 1;
   }
   trace("OptionControl.GetCurrentValueInt( \"" + in_OptionName + "\" ): OptionData don\'t find.");
   return 0;
}
function SetFont(in_FontName)
{
   CommonSetFont(window.textTab1.text,in_FontName);
   CommonSetFont(window.textTab2.text,in_FontName);
   CommonSetFont(window.textTab3.text,in_FontName);
   CommonSetFont(window.back.text.text,in_FontName);
   CommonSetFont(window.apply.text.text,in_FontName);
   CommonSetFont(window.defaultSetting.text.text,in_FontName);
   var _loc3_ = CategoryDataArray[OptionCategoryIndex];
   var _loc2_ = {};
   var _loc1_ = 0;
   while(_loc1_ < _loc3_.OptionDataArray.length)
   {
      _loc2_ = _loc3_.OptionDataArray[_loc1_];
      _loc2_.SetFontFunction(_loc2_,in_FontName);
      _loc1_ = _loc1_ + 1;
   }
}
var InputInitData;
var InputKeyDataArray = new Array();
var InputKeyRepeatIntervalFirst = 20;
var InputKeyRepeatInterval = 10;
var InputKeyRapidInterval = 10;
var INPUT_KEY_CODE_INVALID = -1;
var INPUT_KEY_DATA_INDEX_INVALID = -1;
var InputEnableFlag = false;
var InputKeyStateResetFlag = false;
var InputKeyStateResetCount = 0;
var CategoryDataArray;
var OPTION_CATEGORY_VIDEO = 0;
var OPTION_CATEGORY_AUDIO = 1;
var OPTION_CATEGORY_OTHER = 2;
var OPTION_CATEGORY_NUM = 3;
var OptionCategoryIndex = 0;
var OPTION_TYPE_CATEGORY = 0;
var OPTION_TYPE_ONOFF = 1;
var OPTION_TYPE_SLIDER = 2;
var OPTION_TYPE_LIST = 3;
var OPTION_TYPE_WINDOW_SIZE = 4;
var OPTION_TYPE_BUTTON = 5;
var SettingMCNameArray = new Array("","switchAll","","dialAll","","");
var NextOptionData;
var PrevOptionData;
var bStartUI = false;
var bMainUI = false;
var bChange = false;
var bInitCategoryData = false;
var bApplyAllCategory = true;
trace("Control");
InitKeyData();
var i = 0;
while(i < OPTION_CATEGORY_NUM)
{
   OptionOptionIndexArray[i] = 0;
   i++;
}
gotoAndStop("open");
play();
