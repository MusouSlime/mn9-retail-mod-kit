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
function Mn9BootEnd()
{
   clearInterval(Mn9BootTimer);
   flash.external.ExternalInterface.call("EndPlay");
}
function Init()
{
   InitData = flash.external.ExternalInterface.call("GetInitData");
   trace("Boot.Init: InitData = " + InitData);
   trace("Boot.Init: bSkipEnableComcept = " + InitData.bSkipEnableComcept);
   trace("Boot.Init: bSkipEnableInti = " + InitData.bSkipEnableInti);
   trace("Boot.Init: bSkipEnableUE = " + InitData.bSkipEnableUE);
   trace("Boot.Init: bSkipEnableNotice = " + InitData.bSkipEnableNotice);
   trace("Boot.Init: bSkipEnableCaution = " + InitData.bSkipEnableCaution);
   var _loc0_;
   if((_loc0_ = InitData.PubType) === 1)
   {
      NextIndexLabel[BOOT_INDEX_PUB] = "sc_in";
   }
   InitKeyData();
}
function InitKeyData()
{
   InputInit();
   InputAddKeyDataRepeatDisable("Decide",Skip,function()
   {
   }
   );
   onEnterFrame = InputUpdate;
   InputSetAllKeyDataEnable(false);
}
function Skip()
{
   if(InitData.bConsole)
   {
      if(BootIndex >= BOOT_INDEX_SCALEFORM)
      {
         gotoAndStop("end");
         return undefined;
      }
   }
   gotoAndPlay(NextIndexLabel[BootIndex + 1]);
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
var InitData;
var BOOT_INDEX_COMCEPT = 0;
var BOOT_INDEX_PUB = 1;
var BOOT_INDEX_INTI = 2;
var BOOT_INDEX_UE = 3;
var BOOT_INDEX_SCALEFORM = 4;
var BOOT_INDEX_NVIDIA = 5;
var BOOT_INDEX_NOTICE = 6;
var BOOT_INDEX_CAUTION = 7;
var BOOT_INDEX_END = 8;
var BootIndex = BOOT_INDEX_COMCEPT;
var NextIndexLabel = ["comcept_in","ds_in","inti_in","UE_in","scaleform_in","nvidia_in","notice_in","caution_in","end"];
Init();
BootIndex = BOOT_INDEX_COMCEPT;
InputSetAllKeyDataEnable(false);
Mn9BootTimer = setInterval(this, "Mn9BootEnd", 1000);
