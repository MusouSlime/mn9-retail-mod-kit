// Prototype additions for Option.gfx -> scripts/frame_6/DoAction.as
// Insert inside InitOtherData(), after the RestNum block.
//
// These rows are visual only until the UE3 apply bridge exists. They prove the
// dynamic row layout works because AddOptionDataOnOff attaches a fresh
// "buttonLine" + SettingMCNameArray[Type] clip by name.

   // --- Mighty No. 9 Patcher prototype rows ---
   _loc2_ = AddOptionDataOnOff(_loc1_.length, "mn9mouse",
      { bOn: true, bOnDefault: true, Name: "Free Mouse", GuideInfoArray: undefined });
   _loc2_.UpdateTextFunction = UpdateTextFunctionOnOff;
   _loc1_.push(_loc2_);

   _loc2_ = AddOptionDataOnOff(_loc1_.length, "mn9movies",
      { bOn: false, bOnDefault: false, Name: "Intro Movies", GuideInfoArray: undefined });
   _loc2_.UpdateTextFunction = UpdateTextFunctionOnOff;
   _loc1_.push(_loc2_);

   _loc2_ = AddOptionDataOnOff(_loc1_.length, "mn9voice",
      { bOn: false, bOnDefault: false, Name: "Steam Voice", GuideInfoArray: undefined });
   _loc2_.UpdateTextFunction = UpdateTextFunctionOnOff;
   _loc1_.push(_loc2_);

// Notes:
// - AddOptionDataOnOff stores in_InitData.Name on the OptionData object, but
//   the visible label is drawn by the "buttonLine" clip and comes from the
//   game's TextData. New labels therefore need matching entries in the UE3
//   MN9UIOption text data, or a post-attach override:
//
//   _loc2_.OptionNameMC.text.text = "Free Mouse";
//
//   applied after InitOptionData() has attached the clips.
//
// - Apply path: CreateApplyCategoryData() builds
//   ApplyOptionData[Name] = bOn, then ApplyNewOtherOptionData() sends the
//   object to UE3. Unknown keys are ignored by the TOtherOptionData binder.
//   Wiring these rows needs either new struct fields/native changes or a new
//   ExternalInterface callback.
