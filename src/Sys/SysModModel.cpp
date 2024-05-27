/*
   @title     StarBase
   @file      SysModModel.cpp
   @date      20240411
   @repo      https://github.com/ewowi/StarBase, submit changes to this file as PRs to ewowi/StarBase
   @Authors   https://github.com/ewowi/StarBase/commits/main
   @Copyright © 2024 Github StarBase Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

#include "SysModModel.h"
#include "SysModule.h"
#include "SysModFiles.h"
#include "SysStarJson.h"
#include "SysModUI.h"
#include "SysModInstances.h"

SysModModel::SysModModel() :SysModule("Model") {
  model = new JsonDocument(&allocator);

  JsonArray root = model->to<JsonArray>(); //create

  ppf("Reading model from /model.json... (deserializeConfigFromFS)\n");
  if (files->readObjectFromFile("/model.json", model)) {//not part of success...
    // print->printJson("Read model", *model);
    // web->sendDataWs(*model);
  } else {
    root = model->to<JsonArray>(); //re create the model as it is corrupted by readFromFile
  }
}

void SysModModel::setup() {
  SysModule::setup();

  parentVar = ui->initSysMod(parentVar, name, 4303);
  parentVar["s"] = true; //setup

  ui->initButton(parentVar, "saveModel", false, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
    case f_UIFun:
      ui->setComment(var, "Write to model.json");
      return true;
    case f_ChangeFun:
      doWriteModel = true;
      return true;
    default: return false;
  }});

  #ifdef STARBASE_DEVMODE

  ui->initCheckBox(parentVar, "showObsolete", doShowObsolete, false, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
    case f_UIFun:
      ui->setComment(var, "Show in UI (refresh)");
      return true;
    case f_ChangeFun:
      doShowObsolete = var["value"];
      return true;
    default: return false;
  }});

  ui->initButton(parentVar, "deleteObsolete", false, [](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
    case f_UIFun:
      ui->setLabel(var, "Delete obsolete variables");
      ui->setComment(var, "🚧");
      return true;
    // case f_ChangeFun:
    //   model->to<JsonArray>(); //create
    //   if (files->readObjectFromFile("/model.json", model)) {//not part of success...
    //   }
    //   return true;
    default: return false;
  }});

  #endif //STARBASE_DEVMODE
}

  void SysModModel::loop() {
  // SysModule::loop();

  if (!cleanUpModelDone) { //do after all setups
    cleanUpModelDone = true;
    cleanUpModel();
  }

  if (doWriteModel) {
    ppf("Writing model to /model.json... (serializeConfig)\n");

    // files->writeObjectToFile("/model.json", model);

    cleanUpModel(JsonObject(), false, true);//remove if var["o"] is negative (not cleanedUp) and remove ro values

    StarJson starJson("/model.json", "w"); //open fileName for deserialize
    starJson.addExclusion("fun");
    starJson.addExclusion("dash");
    starJson.addExclusion("o");
    starJson.writeJsonDocToFile(model);

    // print->printJson("Write model", *model); //this shows the model before exclusion

    doWriteModel = false;
  }
}

void SysModModel::cleanUpModel(JsonObject parent, bool oPos, bool ro) {

  JsonArray vars;
  if (parent.isNull()) //no parent
    vars = model->as<JsonArray>();
  else
    vars = varChildren(parent);

  for (JsonArray::iterator varV=vars.begin(); varV!=vars.end(); ++varV) {
  // for (JsonVariant varV : vars) {
    if (varV->is<JsonObject>()) {
      JsonObject var = *varV;

      //no cleanup of o in case of ro value removal
      if (!ro) {
        if (oPos) {
          if (var["o"].isNull() || varOrder(var) >= 0) { //not set negative in initVar
            if (!doShowObsolete) {
              ppf("cleanUpModel remove var %s (""o"">=0)\n", varID(var));          
              vars.remove(varV); //remove the obsolete var (no o or )
            }
          }
          else {
            varOrder(var, -varOrder(var)); //make it possitive
          }
        } else { //!oPos
          if (var["o"].isNull() || varOrder(var) < 0) { 
            ppf("cleanUpModel remove var %s (""o""<0)\n", varID(var));          
            vars.remove(varV); //remove the obsolete var (no o or o is negative - not cleanedUp)
          }
        }
      }

      //remove ro values (ro vars cannot be deleted as SM uses these vars)
      // remove if var is ro or table is instance table (exception here, values don't need to be saved)
      if (ro && (parent["id"] == "insTbl" || varRO(var))) {// && !var["value"].isNull())
        ppf("remove ro value %s\n", varID(var));          
        var.remove("value");
      }

      //recursive call
      if (!varChildren(var).isNull())
        cleanUpModel(var, oPos, ro);
    } 
  }
}

JsonObject SysModModel::findVar(const char * id, JsonArray parent) {
  JsonArray root;
  // print ->print("findVar %s %s\n", id, parent.isNull()?"root":"n");
  if (parent.isNull()) {
    root = model->as<JsonArray>();
    modelParentVar = JsonObject();
  }
  else
    root = parent;

  for (JsonObject var : root) {
    // if (foundVar.isNull()) {
      if (var["id"] == id)
        return var;
      else if (!var["n"].isNull()) {
        JsonObject foundVar = findVar(id, var["n"]);
        if (!foundVar.isNull()) {
          if (modelParentVar.isNull()) modelParentVar = var;  //only recursive lowest assigns parentVar
          // ppf("findvar parent of %s is %s\n", id, varID(modelParentVar));
          return foundVar;
        }
      }
    // }
  }
  return JsonObject();
}

JsonObject SysModModel::findParentVar(const char * id, JsonObject parent) {
  JsonArray varArray;
  // print ->print("findParentVar %s %s\n", id, parent.isNull()?"root":"n");
  if (parent.isNull()) {
    varArray = model->as<JsonArray>();
  }
  else
    varArray = parent["n"];

  JsonObject foundVar = JsonObject();
  for (JsonObject var : varArray) {
    if (foundVar.isNull()) {
      if (var["id"] == id)
        foundVar = parent;
      else if (!var["n"].isNull()) {
        foundVar = findParentVar(id, var);
      }
    }
  }
  return foundVar;
}

void SysModModel::findVars(const char * property, bool value, FindFun fun, JsonArray parent) {
  JsonArray root;
  // print ->print("findVar %s %s\n", id, parent.isNull()?"root":"n");
  if (parent.isNull())
    root = model->as<JsonArray>();
  else
    root = parent;

  for (JsonObject var : root) {
    if (var[property] == value)
      fun(var);
    if (!var["n"].isNull())
      findVars(property, value, fun, var["n"]);
  }
}

void SysModModel::varToValues(JsonObject var, JsonArray row) {

    //add value for each child
    // JsonArray row = rows.add<JsonArray>();
    for (JsonObject childVar : var["n"].as<JsonArray>()) {
      print->printJson("fxTbl childs", childVar);
      row.add(childVar["value"]);

      if (!childVar["n"].isNull()) {
        varToValues(childVar, row.add<JsonArray>());
      }
    }
}

bool checkDash(JsonObject var) {
  if (var["dash"])
    return true;
  else {
    JsonObject parentVar = mdl->findParentVar(var["id"]);
    if (!parentVar.isNull())
      return checkDash(parentVar);
  }
  return false;
}

void SysModModel::setValueChangeFun(JsonObject var, unsigned8 rowNr) {
  //done here as ui cannot be used in SysModModel.h
  if (checkDash(var))
    instances->changedVarsQueue.push_back(var);

  ui->callVarFun(var, rowNr, f_ChangeFun);

  // web->sendResponseObject();
}  