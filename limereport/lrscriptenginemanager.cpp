/***************************************************************************
 *   This file is part of the Lime Report project                          *
 *   Copyright (C) 2015 by Alexander Arin                                  *
 *   arin_a@bk.ru                                                          *
 *                                                                         *
 **                   GNU General Public License Usage                    **
 *                                                                         *
 *   This library is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 **                  GNU Lesser General Public License                    **
 *                                                                         *
 *   This library is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation, either version 3 of the    *
 *   License, or (at your option) any later version.                       *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library.                                      *
 *   If not, see <http://www.gnu.org/licenses/>.                           *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 ****************************************************************************/
#include "lrscriptenginemanager.h"

#include <QDate>
#include <QStringList>
#include <QScriptValueIterator>
#include <QMessageBox>
#ifdef HAVE_UI_LOADER
#include <QUiLoader>
#include <QBuffer>
#include <QWidget>
#endif
#include "lrdatasourcemanager.h"
#include "lrbasedesignintf.h"
#include "lrbanddesignintf.h"

Q_DECLARE_METATYPE(QColor)
Q_DECLARE_METATYPE(QFont)
Q_DECLARE_METATYPE(LimeReport::ScriptEngineManager *)

QScriptValue constructColor(QScriptContext *context, QScriptEngine *engine)
{
     QColor color(context->argument(0).toString());
     return engine->toScriptValue(color);
}

namespace LimeReport{

ScriptEngineNode::~ScriptEngineNode()
{
    for (int i = 0; i<m_childs.count(); ++i){
        delete m_childs[i];
    }
}

ScriptEngineNode*ScriptEngineNode::addChild(const QString& name, const QString& description,  ScriptEngineNode::NodeType type, const QIcon& icon)
{
    ScriptEngineNode* res = new ScriptEngineNode(name, description, type,this,icon);
    m_childs.push_back(res);
    return res;
}

int ScriptEngineNode::row()
{
    if (m_parent){
        return m_parent->m_childs.indexOf(const_cast<ScriptEngineNode*>(this));
    }
    return 0;
}

void ScriptEngineNode::clear()
{
    for (int i=0; i<m_childs.count(); ++i){
        delete m_childs[i];
    }
    m_childs.clear();
}

ScriptEngineModel::ScriptEngineModel(ScriptEngineManager* scriptManager)
    :m_rootNode(new ScriptEngineNode())
{
    setScriptEngineManager(scriptManager);
}

ScriptEngineModel::~ScriptEngineModel() {
    delete m_rootNode;
}

QModelIndex ScriptEngineModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) return QModelIndex();

    ScriptEngineNode* childNode = nodeFromIndex(child);
    if (!childNode) return QModelIndex();

    ScriptEngineNode* parentNode = childNode->parent();
    if ((parentNode == m_rootNode) || (!parentNode)) return QModelIndex();
    return createIndex(parentNode->row(),0,parentNode);
}

QModelIndex ScriptEngineModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!m_rootNode)
        return QModelIndex();

    if (!hasIndex(row,column,parent))
        return QModelIndex();

    ScriptEngineNode* parentNode;
    if (parent.isValid()){
        parentNode = nodeFromIndex(parent);
    } else {
        parentNode = m_rootNode;
    }

    ScriptEngineNode* childNode = parentNode->child(row);
    if (childNode){
        return createIndex(row,column,childNode);
    } else return QModelIndex();
}

int ScriptEngineModel::rowCount(const QModelIndex& parent) const
{
    if (!m_rootNode) return 0;
    ScriptEngineNode* parentNode;
    if (parent.isValid())
        parentNode = nodeFromIndex(parent);
    else
        parentNode = m_rootNode;
    return parentNode->childCount();
}

int ScriptEngineModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant ScriptEngineModel::data(const QModelIndex& index, int role) const
{
    ScriptEngineNode *node = nodeFromIndex(index);
    switch (role) {
    case Qt::DisplayRole:
        if (!node) return QVariant();
        return node->name();
        break;
    case Qt::DecorationRole :
        if (!node) return QIcon();
        return node->icon();
        break;
    default:
        return QVariant();
    }
}

void ScriptEngineModel::setScriptEngineManager(ScriptEngineManager* scriptManager)
{
    m_scriptManager = scriptManager;
    updateModel();
}

void ScriptEngineModel::slotScriptEngineChanged()
{
    updateModel();
}

ScriptEngineNode*ScriptEngineModel::nodeFromIndex(const QModelIndex& index) const
{
    if (index.isValid()){
        return static_cast<ScriptEngineNode*>(index.internalPointer());
    } else return m_rootNode;
}

void ScriptEngineModel::updateModel()
{
    beginResetModel();
    m_rootNode->clear();
    QMap<QString,ScriptEngineNode*> categories;
    foreach(ScriptFunctionDesc funcDesc, m_scriptManager->functionsDescribers()){
        ScriptEngineNode* categ;
        QString categoryName = (!funcDesc.category.isEmpty())?funcDesc.category:"NO CATEGORY";
        if (categories.contains(categoryName)){
            categ = categories.value(categoryName);
        } else {
            categ = m_rootNode->addChild(categoryName,"",ScriptEngineNode::Category,QIcon(":/report/images/folder"));
            categories.insert(categoryName,categ);
        }
        categ->addChild(funcDesc.name,funcDesc.description,ScriptEngineNode::Function,QIcon(":/report/images/function"));
    }
    endResetModel();
}

ScriptEngineManager::~ScriptEngineManager()
{
    delete m_model;
    m_model = 0;
    delete m_scriptEngine;
}

bool ScriptEngineManager::isFunctionExists(const QString &functionName) const
{
    foreach (ScriptFunctionDesc desc, m_functions) {
        if (desc.name.compare(functionName,Qt::CaseInsensitive)==0){
            return true;
        }
    }
    return false;
}

void ScriptEngineManager::deleteFunction(const QString &functionsName)
{
    QMutableListIterator<ScriptFunctionDesc> it(m_functions);
    while(it.hasNext()){
        if (it.next().name.compare(functionsName, Qt::CaseInsensitive)==0){
            it.remove();
        }
    }
}

bool ScriptEngineManager::addFunction(const JSFunctionDesc &functionDescriber)
{
    ScriptValueType functionManager = scriptEngine()->globalObject().property(functionDescriber.managerName());
#ifdef USE_QJSENGINE
    if (functionManager.isUndefined()){
#else
    if (!functionManager.isValid()){
#endif
        functionManager = scriptEngine()->newQObject(functionDescriber.manager());
        scriptEngine()->globalObject().setProperty(
                    functionDescriber.managerName(),
                    functionManager
        );
    }

    if (functionManager.toQObject() == functionDescriber.manager()){
        ScriptValueType checkWrapper = scriptEngine()->evaluate(functionDescriber.scriptWrapper());
        if (!checkWrapper.isError()){
            ScriptFunctionDesc funct;
            funct.name = functionDescriber.name();
            funct.description = functionDescriber.description();
            funct.category = functionDescriber.category();
            funct.type = ScriptFunctionDesc::Native;
            m_functions.append(funct);
            if (m_model)
                m_model->updateModel();
            return true;
        } else {
            m_lastError = checkWrapper.toString();
            return false;
        }
    } else {
        m_lastError = tr("Function manger with name \"%1\" already exists!");
        return false;
    }

}

bool ScriptEngineManager::containsFunction(const QString& functionName){
    foreach (ScriptFunctionDesc funct, m_functions) {
        if (funct.name.compare(functionName)== 0){
            return true;
        }
    }
    return false;
}

#ifndef USE_QJSENGINE
Q_DECL_DEPRECATED bool ScriptEngineManager::addFunction(const QString& name,
                                              QScriptEngine::FunctionSignature function,
                                              const QString& category,
                                              const QString& description)
{
    if (!containsFunction(name)){
        ScriptFunctionDesc funct;
        funct.name = name;
        funct.description = description;
        funct.category = category;
        funct.scriptValue = scriptEngine()->newFunction(function);
        funct.scriptValue.setProperty("functionName",name);
        funct.scriptValue.setData(m_scriptEngine->toScriptValue(this));
        funct.type = ScriptFunctionDesc::Native;
        m_functions.append(funct);
        if (m_model)
            m_model->updateModel();
        m_scriptEngine->globalObject().setProperty(funct.name,funct.scriptValue);
        return true;
    } else {
        return false;
    }
}
#endif

bool ScriptEngineManager::addFunction(const QString& name, const QString& script, const QString& category, const QString& description)
{
    ScriptValueType functionValue = m_scriptEngine->evaluate(script);
    if (!functionValue.isError()){
        ScriptFunctionDesc funct;
        funct.scriptValue = functionValue;
        funct.name =  name;
        funct.category = category;
        funct.description = description;
        funct.type = ScriptFunctionDesc::Script;
        m_functions.append(funct);
        m_model->updateModel();        
        return true;
    } else {
        m_lastError = functionValue.toString();
        return false;
    }
}

QStringList ScriptEngineManager::functionsNames()
{
    QStringList res;
    foreach(ScriptFunctionDesc func, m_functions){
        res<<func.name;
    }
    return res;
}

void ScriptEngineManager::setDataManager(DataSourceManager *dataManager){
    if (m_dataManager != dataManager){
        m_dataManager =  dataManager;
        if (m_dataManager){
            foreach(QString func, m_dataManager->groupFunctionNames()){
                JSFunctionDesc describer(
                    func,
                    tr("GROUP FUNCTIONS"),
                    func+"(\""+tr("FieldName")+"\",\""+tr("BandName")+"\")",
                    LimeReport::Const::FUNCTION_MANAGER_NAME,
                    m_functionManager,
                    QString("function %1(fieldName,bandName){\
                            return %2.calcGroupFunction(\"%1\",fieldName,bandName);}"
                    ).arg(func)
                     .arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                );
                addFunction(describer);
            }

//            qDebug()<<"is script context exists before set datamanager is called"<< (m_context == 0);

//            ICallbackDatasource* tableOfContens = m_dataManager->createCallbackDatasource("tableofcontens");
//            connect(tableOfContens, SIGNAL(getCallbackData(LimeReport::CallbackInfo,QVariant&)),
//                    m_tableOfContens, SLOT(slotOneSlotDS(LimeReport::CallbackInfo,QVariant&)));
        }
    }
}

QString ScriptEngineManager::expandUserVariables(QString context, RenderPass pass, ExpandType expandType, QVariant &varValue)
{
    QRegExp rx(Const::VARIABLE_RX);
    if (context.contains(rx)){
        int pos = 0;
        while ((pos = rx.indexIn(context,pos))!=-1){
            QString variable=rx.cap(1);
            pos += rx.matchedLength();
            if (dataManager()->containsVariable(variable) ){
                try {

                    varValue = dataManager()->variable(variable);
                    switch (expandType){
                    case EscapeSymbols:
                        context.replace(rx.cap(0),escapeSimbols(varValue.toString()));
                    break;
                    case NoEscapeSymbols:
                        context.replace(rx.cap(0),varValue.toString());
                    break;
                    case ReplaceHTMLSymbols:
                        context.replace(rx.cap(0),replaceHTMLSymbols(varValue.toString()));
                    break;
                    }
                    pos=0;

                } catch (ReportError e){
                    dataManager()->putError(e.what());
                    if (!dataManager()->reportSettings() || dataManager()->reportSettings()->suppressAbsentFieldsAndVarsWarnings())
                        context.replace(rx.cap(0),e.what());
                    else
                        context.replace(rx.cap(0),"");
                }
            } else {
                QString error;
                error = tr("Variable %1 not found").arg(variable);
                dataManager()->putError(error);
                if (!dataManager()->reportSettings() || dataManager()->reportSettings()->suppressAbsentFieldsAndVarsWarnings())
                    context.replace(rx.cap(0),error);
                else
                    context.replace(rx.cap(0),"");
            }
        }
    }
    return context;
}

QString ScriptEngineManager::expandDataFields(QString context, ExpandType expandType, QVariant &varValue, QObject *reportItem)
{
    QRegExp rx(Const::FIELD_RX);

    if (context.contains(rx)){
        while ((rx.indexIn(context))!=-1){
            QString field=rx.cap(1);

            if (dataManager()->containsField(field)) {
                QString fieldValue;
                varValue = dataManager()->fieldData(field);
                if (expandType == EscapeSymbols) {
                    if (dataManager()->fieldData(field).isNull()) {
                        fieldValue="\"\"";
                    } else {
                        fieldValue = escapeSimbols(varValue.toString());
                        switch (dataManager()->fieldData(field).type()) {
                        case QVariant::Char:
                        case QVariant::String:
                        case QVariant::StringList:
                        case QVariant::Date:
                        case QVariant::DateTime:
                            fieldValue = "\""+fieldValue+"\"";
                            break;
                        default:
                            break;
                        }
                    }
                } else {
                    if (expandType == ReplaceHTMLSymbols)
                        fieldValue = replaceHTMLSymbols(varValue.toString());
                    else fieldValue = varValue.toString();
                }
                if (varValue.isValid())
                    context.replace(rx.cap(0),fieldValue);

            } else {
                QString error;
                if (reportItem){
                    error = tr("Field %1 not found in %2!").arg(field).arg(reportItem->objectName());
                    dataManager()->putError(error);
                }
                varValue = QVariant();
                if (!dataManager()->reportSettings() || !dataManager()->reportSettings()->suppressAbsentFieldsAndVarsWarnings())
                    context.replace(rx.cap(0),error);
                else
                    context.replace(rx.cap(0),"");
            }
        }
    }

    return context;
}

QString ScriptEngineManager::expandScripts(QString context, QVariant& varValue, QObject *reportItem)
{
    QRegExp rx(Const::SCRIPT_RX);

    if (context.contains(rx)){

        if (ScriptEngineManager::instance().dataManager()!=dataManager())
            ScriptEngineManager::instance().setDataManager(dataManager());

        ScriptEngineType* se = ScriptEngineManager::instance().scriptEngine();

        if (reportItem){

            ScriptValueType svThis;

#ifdef USE_QJSENGINE
            svThis = getCppOwnedJSValue(*se, reportItem);
            se->globalObject().setProperty("THIS",svThis);
#else
            svThis = se->globalObject().property("THIS");
            if (svThis.isValid()){
                se->newQObject(svThis, reportItem);
            } else {
                svThis = se->newQObject(reportItem);
                se->globalObject().setProperty("THIS",svThis);
            }
#endif
        }

        ScriptExtractor scriptExtractor(context);
        if (scriptExtractor.parse()){
            for(int i=0; i<scriptExtractor.count();++i){
                QString scriptBody = expandDataFields(scriptExtractor.bodyAt(i),EscapeSymbols, varValue, reportItem);
                scriptBody = expandUserVariables(scriptBody, FirstPass, EscapeSymbols, varValue);
                ScriptValueType value = se->evaluate(scriptBody);
#ifdef USE_QJSENGINE
                if (!value.isError()){
                    varValue = value.toVariant();
                    context.replace(scriptExtractor.scriptAt(i),value.toString());
                } else {
                    context.replace(scriptExtractor.scriptAt(i),value.toString());
                }
#else
                if (!se->hasUncaughtException()) {
                    varValue = value.toVariant();
                    context.replace(scriptExtractor.scriptAt(i),value.toString());
                } else {
                    context.replace(scriptExtractor.scriptAt(i),se->uncaughtException().toString());
                }
#endif
            }
        }
    }

    return context;
}

QVariant ScriptEngineManager::evaluateScript(const QString& script){

    QRegExp rx(Const::SCRIPT_RX);
    QVariant varValue;

    if (script.contains(rx)){

        if (ScriptEngineManager::instance().dataManager()!=dataManager())
            ScriptEngineManager::instance().setDataManager(dataManager());

        ScriptEngineType* se = ScriptEngineManager::instance().scriptEngine();

        ScriptExtractor scriptExtractor(script);
        if (scriptExtractor.parse()){
            QString scriptBody = expandDataFields(scriptExtractor.bodyAt(0),EscapeSymbols, varValue, 0);
            scriptBody = expandUserVariables(scriptBody, FirstPass, EscapeSymbols, varValue);
            ScriptValueType value = se->evaluate(scriptBody);
#ifdef USE_QJSENGINE
            if (!value.isError()){
#else
            if (!se->hasUncaughtException()) {
#endif
                return value.toVariant();
            }
        }
    }
    return QVariant();
}

void ScriptEngineManager::addTableOfContensItem(const QString& uniqKey, const QString& content, int indent)
{
    Q_ASSERT(m_context != 0);
    if (m_context){
        BandDesignIntf* currentBand = m_context->getCurrentBand();
        m_context->tableOfContens()->setItem(uniqKey, content, 0, indent);
        if (currentBand)
            currentBand->addBookmark(uniqKey, content);
    }
}

void ScriptEngineManager::clearTableOfContens(){
    if (m_context) {
        if (m_context->tableOfContens())
            m_context->tableOfContens()->clear();
    }
}

void ScriptEngineManager::updateModel()
{

}

bool ScriptEngineManager::createLineFunction()
{
    JSFunctionDesc fd;

    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("SYSTEM"));
    fd.setName("line");
    fd.setDescription("line(\""+tr("BandName")+"\")");
    fd.setScriptWrapper(QString("function line(bandName){ return %1.line(bandName);}").arg(LimeReport::Const::FUNCTION_MANAGER_NAME));

    return addFunction(fd);

}

bool ScriptEngineManager::createNumberFomatFunction()
{
    JSFunctionDesc fd;

    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("NUMBER"));
    fd.setName("numberFormat");
    fd.setDescription("numberFormat(\""+tr("Value")+"\",\""+tr("Format")+"\",\""+
                      tr("Precision")+"\",\""+
                      tr("Locale")+"\")"
                      );
    fd.setScriptWrapper(QString("function numberFormat(value, format, precision, locale){"
                                " if(typeof(format)==='undefined') format = \"f\"; "
                                " if(typeof(precision)==='undefined') precision=2; "
                                " if(typeof(locale)==='undefined') locale=\"\"; "
                                "return %1.numberFormat(value,format,precision,locale);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createDateFormatFunction(){
//    addFunction("dateFormat",dateFormat,"DATE&TIME", "dateFormat(\""+tr("Value")+"\",\""+tr("Format")+"\")");
    JSFunctionDesc fd;

    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("DATE&TIME"));
    fd.setName("dateFormat");
    fd.setDescription("dateFormat(\""+tr("Value")+"\",\""+tr("Format")+"\")");
    fd.setScriptWrapper(QString("function dateFormat(value, format){"
                                " if(typeof(format)==='undefined') format = \"dd.MM.yyyy\"; "
                                "return %1.dateFormat(value,format);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createTimeFormatFunction(){
//    addFunction("timeFormat",timeFormat,"DATE&TIME", "dateFormat(\""+tr("Value")+"\",\""+tr("Format")+"\")");
    JSFunctionDesc fd;

    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("DATE&TIME"));
    fd.setName("timeFormat");
    fd.setDescription("timeFormat(\""+tr("Value")+"\",\""+tr("Format")+"\")");
    fd.setScriptWrapper(QString("function timeFormat(value, format){"
                                " if(typeof(format)==='undefined') format = \"hh:mm\"; "
                                "return %1.timeFormat(value,format);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createDateTimeFormatFunction(){
//    addFunction("dateTimeFormat", dateTimeFormat, "DATE&TIME", "dateTimeFormat(\""+tr("Value")+"\",\""+tr("Format")+"\")");
    JSFunctionDesc fd;

    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("DATE&TIME"));
    fd.setName("dateTimeFormat");
    fd.setDescription("dateTimeFormat(\""+tr("Value")+"\",\""+tr("Format")+"\")");
    fd.setScriptWrapper(QString("function dateTimeFormat(value, format){"
                                " if(typeof(format)==='undefined') format = \"dd.MM.yyyy hh:mm\"; "
                                "return %1.dateTimeFormat(value,format);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createDateFunction(){
//    addFunction("date",date,"DATE&TIME","date()");
    JSFunctionDesc fd;

    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("DATE&TIME"));
    fd.setName("date");
    fd.setDescription("date()");
    fd.setScriptWrapper(QString("function date(){"
                                "return %1.date();}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}


bool ScriptEngineManager::createNowFunction(){
//    addFunction("now",now,"DATE&TIME","now()");
    JSFunctionDesc fd;

    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("DATE&TIME"));
    fd.setName("now");
    fd.setDescription("now()");
    fd.setScriptWrapper(QString("function now(){"
                                "return %1.now();}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createCurrencyFormatFunction(){
//    addFunction("currencyFormat",currencyFormat,"NUMBER","currencyFormat(\""+tr("Value")+"\",\""+tr("Locale")+"\")");
    JSFunctionDesc fd;

    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("NUMBER"));
    fd.setName("currencyFormat");
    fd.setDescription("currencyFormat(\""+tr("Value")+"\",\""+tr("Locale")+"\")");
    fd.setScriptWrapper(QString("function currencyFormat(value, locale){"
                                " if(typeof(locale)==='undefined') locale = \"\"; "
                                "return %1.currencyFormat(value,locale);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createCurrencyUSBasedFormatFunction(){
//    addFunction("currencyUSBasedFormat",currencyUSBasedFormat,"NUMBER","currencyUSBasedFormat(\""+tr("Value")+",\""+tr("CurrencySymbol")+"\")");
    JSFunctionDesc fd;

    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("NUMBER"));
    fd.setName("currencyUSBasedFormat");
    fd.setDescription("currencyUSBasedFormat(\""+tr("Value")+",\""+tr("CurrencySymbol")+"\")");
    fd.setScriptWrapper(QString("function currencyUSBasedFormat(value, currencySymbol){"
                                " if(typeof(currencySymbol)==='undefined') currencySymbol = \"\"; "
                                "return %1.currencyFormat(value,currencySymbol);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createSetVariableFunction(){
//    addFunction("setVariable", setVariable, "GENERAL", "setVariable(\""+tr("Name")+"\",\""+tr("Value")+"\")");
    JSFunctionDesc fd;

    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("GENERAL"));
    fd.setName("setVariable");
    fd.setDescription("setVariable(\""+tr("Name")+"\",\""+tr("Value")+"\")");
    fd.setScriptWrapper(QString("function setVariable(name, value){"
                                "return %1.setVariable(name,value);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createGetVariableFunction()
{
    JSFunctionDesc fd;
    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("GENERAL"));
    fd.setName("getVariable");
    fd.setDescription("getVariable(\""+tr("Name")+"\")");
    fd.setScriptWrapper(QString("function getVariable(name){"
                                "return %1.getVariable(name);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createGetFieldFunction()
{
    JSFunctionDesc fd;
    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("GENERAL"));
    fd.setName("getField");
    fd.setDescription("getField(\""+tr("Name")+"\")");
    fd.setScriptWrapper(QString("function getField(name){"
                                "return %1.getField(name);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createGetFieldByKeyFunction()
{
    JSFunctionDesc fd;
    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("GENERAL"));
    fd.setName("getFieldByKeyField");
    fd.setDescription("getFieldByKeyField(\""+tr("Datasource")+"\", \""+
                      tr("ValueField")+"\",\""+
                      tr("KeyField")+"\", \""+
                      tr("KeyFieldValue")+"\")"
    );
    fd.setScriptWrapper(QString("function getFieldByKeyField(datasource, valueFieldName, keyFieldName, keyValue){"
                                "return %1.getFieldByKeyField(datasource, valueFieldName, keyFieldName, keyValue);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createAddTableOfContensItemFunction()
{
    JSFunctionDesc fd;
    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("GENERAL"));
    fd.setName("addTableOfContensItem");
    fd.setDescription("addTableOfContensItem(\""+tr("Unique identifier")+" \""+tr("Content")+"\", \""+tr("Indent")+"\")");
    fd.setScriptWrapper(QString("function addTableOfContensItem(uniqKey, content, indent){"
                                "return %1.addTableOfContensItem(uniqKey, content, indent);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createClearTableOfContensFunction()
{
    JSFunctionDesc fd;
    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("GENERAL"));
    fd.setName("clearTableOfContens");
    fd.setDescription("clearTableOfContens()");
    fd.setScriptWrapper(QString("function clearTableOfContens(){"
                                "return %1.clearTableOfContens();}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

bool ScriptEngineManager::createReopenDatasourceFunction()
{
    JSFunctionDesc fd;
    fd.setManager(m_functionManager);
    fd.setManagerName(LimeReport::Const::FUNCTION_MANAGER_NAME);
    fd.setCategory(tr("GENERAL"));
    fd.setName("reopenDatasource");
    fd.setDescription("reopenDatasource(\""+tr("datasourceName")+"\")");
    fd.setScriptWrapper(QString("function reopenDatasource(datasourceName){"
                                "return %1.reopenDatasource(datasourceName);}"
                               ).arg(LimeReport::Const::FUNCTION_MANAGER_NAME)
                        );
    return addFunction(fd);
}

ScriptEngineManager::ScriptEngineManager()
    :m_model(0), m_dataManager(0)
{
    m_scriptEngine = new ScriptEngineType;
    m_functionManager = new ScriptFunctionsManager(this);
    m_functionManager->setScriptEngineManager(this);
#ifndef USE_QJSENGINE
    m_scriptEngine->setDefaultPrototype(qMetaTypeId<QComboBox*>(),
                                  m_scriptEngine->newQObject(new ComboBoxPrototype()));
#endif

    createLineFunction();
    createNumberFomatFunction();
    createDateFormatFunction();
    createTimeFormatFunction();
    createDateTimeFormatFunction();
    createDateFunction();
    createNowFunction();
#if QT_VERSION>0x040800
    createCurrencyFormatFunction();
    createCurrencyUSBasedFormatFunction();
#endif
    createSetVariableFunction();
    createGetFieldFunction();
    createGetFieldByKeyFunction();
    createGetVariableFunction();
#ifndef USE_QJSENGINE
    QScriptValue colorCtor = m_scriptEngine->newFunction(constructColor);
    m_scriptEngine->globalObject().setProperty("QColor", colorCtor);

    QScriptValue fontProto(m_scriptEngine->newQObject(new QFontPrototype,QScriptEngine::ScriptOwnership));
    m_scriptEngine->setDefaultPrototype(qMetaTypeId<QFont>(), fontProto);
    QScriptValue fontConstructor = m_scriptEngine->newFunction(QFontPrototype::constructorQFont, fontProto);
    m_scriptEngine->globalObject().setProperty("QFont", fontConstructor);
#endif
    createAddTableOfContensItemFunction();
    createClearTableOfContensFunction();
    createReopenDatasourceFunction();

    m_model = new ScriptEngineModel(this);
}

bool ScriptExtractor::parse()
{
    int currentPos = 0;
    parse(currentPos,None);
    return m_scriptsBody.count()>0;

}

bool ScriptExtractor::parse(int &curPos,const State& state)
{
    while (curPos<m_context.length()){
        switch (state) {
        case OpenBracketFound:
            if (m_context[curPos]=='}'){
                return true;
            } else {
                if (m_context[curPos]=='{')
                   extractBracket(curPos);
            }
        case None:
            if (m_context[curPos]=='$'){
                int startPos = curPos;
                if (isStartScriptLexem(curPos))
                    extractScript(curPos,substring(m_context,startPos,curPos));
                if (isStartFieldLexem(curPos) || isStartVariableLexem(curPos))
                    skipField(curPos);
            }
        default:
            break;
        }
        curPos++;
    }
    return false;
}

void ScriptExtractor::extractScript(int &curPos, const QString& startStr)
{
    int startPos = curPos;
    if (extractBracket(curPos)){
        QString scriptBody = substring(m_context,startPos+1,curPos);
        m_scriptsBody.push_back(scriptBody);
        m_scriptsStartLex.push_back(startStr+'{');
    }
}

void ScriptExtractor::skipField(int &curPos){
    while (curPos<m_context.length()) {
        if (m_context[curPos]=='}'){
            return;
        } else {
            curPos++;
        }
    }
}

bool ScriptExtractor::extractBracket(int &curPos)
{
    curPos++;
    return parse(curPos,OpenBracketFound);
}

bool ScriptExtractor::isStartLexem(int& curPos, QChar value){
    int pos = curPos+1;
    State ls = BuksFound;
    while (pos<m_context.length()){
        switch (ls){
        case BuksFound:
            if (m_context[pos]==value){
                ls = SignFound;
            } else {
                if (m_context[pos]!=' ')
                    return false;
            }
            break;
        case SignFound:
            if (m_context[pos]=='{'){
                curPos=pos;
                return true;
            } else
                if (m_context[pos]!=' ')
                    return false;
        default:
            break;
        }
        pos++;
    }
    return false;
}

bool ScriptExtractor::isStartScriptLexem(int& curPos)
{
    return isStartLexem(curPos, SCRIPT_SIGN);
}

bool ScriptExtractor::isStartFieldLexem(int& curPos){
    return isStartLexem(curPos, FIELD_SIGN);
}

bool ScriptExtractor::isStartVariableLexem(int &curPos)
{
    return isStartLexem(curPos, VARIABLE_SIGN);
}


QString ScriptExtractor::substring(const QString &value, int start, int end)
{
    return value.mid(start,end-start);
}

QString DialogDescriber::name() const
{
    return m_name;
}

void DialogDescriber::setName(const QString& name)
{
    m_name = name;
}

QByteArray DialogDescriber::description() const
{
    return m_description;
}

void DialogDescriber::setDescription(const QByteArray &description)
{
    m_description = description;
}

#ifdef HAVE_UI_LOADER
void ScriptEngineContext::addDialog(const QString& name, const QByteArray& description)
{
    m_dialogs.push_back(DialogDescriber::create(name,description));
    emit dialogAdded(name);
}

bool ScriptEngineContext::changeDialog(const QString& name, const QByteArray& description)
{
    foreach( DialogDescriber::Ptr describer, m_dialogs){
        if (describer->name().compare(name) == 0){
            describer->setDescription(description);
            {
                QList<DialogPtr>::Iterator it = m_createdDialogs.begin();
                while(it!=m_createdDialogs.end()){
                    if ((*it)->objectName()==name){
                        it = m_createdDialogs.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            return true;
        }
    }
    return false;
}

bool ScriptEngineContext::changeDialogName(const QString& oldName, const QString& newName)
{
    foreach( DialogDescriber::Ptr describer, m_dialogs){
        if (describer->name().compare(oldName) == 0){
            describer->setName(newName);
            {
                QList<DialogPtr>::Iterator it = m_createdDialogs.begin();
                while(it!=m_createdDialogs.end()){
                    if ((*it)->objectName()==oldName){
                        it = m_createdDialogs.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            return true;
        }
    }
    return false;
}

bool ScriptEngineContext::previewDialog(const QString& dialogName)
{
    QDialog* dialog = getDialog(dialogName);
    if (dialog) {
        dialog->exec();
        return true;
    } else {
        m_lastError = tr("Dialog with name: %1 can`t be created").arg(dialogName);
        return false;
    }
}

bool ScriptEngineContext::containsDialog(const QString& dialogName)
{
    foreach(DialogDescriber::Ptr dialog, m_dialogs){
        if (dialog->name()==dialogName)
            return true;
    }
    return false;
}

void ScriptEngineContext::deleteDialog(const QString& dialogName)
{
    {
        QVector<DialogDescriber::Ptr>::Iterator it = m_dialogs.begin();
        while(it!=m_dialogs.end()){
            if ((*it)->name()==dialogName){
                it = m_dialogs.erase(it);
                emit dialogDeleted(dialogName);
            } else {
                ++it;
            }
        }
    }
    {
        QList<DialogPtr>::Iterator it = m_createdDialogs.begin();
        while(it!=m_createdDialogs.end()){
            if ((*it)->objectName()==dialogName){
                it = m_createdDialogs.erase(it);
            } else {
                ++it;
            }
        }
    }
}

#endif

void ScriptEngineContext::clear()
{
#ifdef HAVE_UI_LOADER
    m_dialogs.clear();
    m_createdDialogs.clear();
#endif
    m_initScript.clear();
    m_tableOfContens->clear();
    m_lastError="";
}

QObject* ScriptEngineContext::createElement(const QString& collectionName, const QString& elementType)
{
    Q_UNUSED(elementType)
#ifdef HAVE_UI_LOADER
    if (collectionName.compare("dialogs",Qt::CaseInsensitive)==0){
        m_dialogs.push_back(DialogDescriber::create());
        return m_dialogs.at(m_dialogs.count()-1).data();
    }
#else
    Q_UNUSED(collectionName)
#endif
    return 0;
}

int ScriptEngineContext::elementsCount(const QString& collectionName)
{
#ifdef HAVE_UI_LOADER
    if (collectionName.compare("dialogs",Qt::CaseInsensitive)==0){
        return m_dialogs.count();
    };
#else
    Q_UNUSED(collectionName)
#endif
    return 0;
}

QObject* ScriptEngineContext::elementAt(const QString& collectionName, int index)
{
#ifdef HAVE_UI_LOADER
    if (collectionName.compare("dialogs",Qt::CaseInsensitive)==0){
        return m_dialogs.at(index).data();
    };
#else
    Q_UNUSED(collectionName)
    Q_UNUSED(index)
#endif
    return 0;
}

void ScriptEngineContext::collectionLoadFinished(const QString& collectionName)
{
    Q_UNUSED(collectionName);
}

#ifdef HAVE_UI_LOADER
QDialog* ScriptEngineContext::createDialog(DialogDescriber* cont)
{
    QUiLoader loader;
    QByteArray desc = cont->description();
    QBuffer buffer(&desc);
    buffer.open(QIODevice::ReadOnly);
    QDialog* dialog = dynamic_cast<QDialog*>(loader.load(&buffer));
    m_createdDialogs.push_back(QSharedPointer<QDialog>(dialog));
    if (cont->name().compare(dialog->objectName())){
        cont->setName(dialog->objectName());
        emit dialogNameChanged(dialog->objectName());
    }
    return dialog;
}

QDialog* ScriptEngineContext::findDialog(const QString& dialogName)
{
    foreach(DialogPtr dialog, m_createdDialogs){
        if (dialog->objectName()==dialogName)
            return dialog.data();
    }
    return 0;
}

DialogDescriber* ScriptEngineContext::findDialogContainer(const QString& dialogName)
{
    foreach (DialogDescriber::Ptr dialogCont , m_dialogs) {
        if (dialogCont->name().compare(dialogName,Qt::CaseInsensitive)==0){
            return dialogCont.data();
        }
    }
    return 0;
}

TableOfContens* ScriptEngineContext::tableOfContens() const
{
    return m_tableOfContens;
}

void ScriptEngineContext::setTableOfContens(TableOfContens* tableOfContens)
{
    m_tableOfContens = tableOfContens;
}

PageItemDesignIntf* ScriptEngineContext::getCurrentPage() const
{
    return m_currentPage;
}

void ScriptEngineContext::setCurrentPage(PageItemDesignIntf* currentPage)
{
    m_currentPage = currentPage;
}

BandDesignIntf* ScriptEngineContext::getCurrentBand() const
{
    return m_currentBand;
}

void ScriptEngineContext::setCurrentBand(BandDesignIntf* currentBand)
{
    m_currentBand = currentBand;
}

QDialog* ScriptEngineContext::getDialog(const QString& dialogName)
{
    QDialog* dialog = findDialog(dialogName);
    if (dialog){
        return dialog;
    } else {
        DialogDescriber* cont = findDialogContainer(dialogName);
        if (cont){
            dialog = createDialog(cont);
            if (dialog)
                return dialog;
        }
    }
    return 0;
}

QString ScriptEngineContext::getNewDialogName()
{
    QString result = "Dialog";
    int index = m_dialogs.size() - 1;
    while (containsDialog(result)){
        index++;
        result = QString("Dialog%1").arg(index);
    }
    return result;
}

#endif

void ScriptEngineContext::baseDesignIntfToScript(const QString& pageName, BaseDesignIntf* item)
{
    if ( item ) {
        if (item->metaObject()->indexOfSignal("beforeRender()")!=-1)
            item->disconnect(SIGNAL(beforeRender()));
        if (item->metaObject()->indexOfSignal("afterData()")!=-1)
            item->disconnect(SIGNAL(afterData()));
        if (item->metaObject()->indexOfSignal("afterRender()")!=-1)
            item->disconnect(SIGNAL(afterRender()));

        ScriptEngineType* engine = ScriptEngineManager::instance().scriptEngine();

#ifdef USE_QJSENGINE
        ScriptValueType sItem = getCppOwnedJSValue(*engine, item);
        engine->globalObject().setProperty(pageName+"_"+item->patternName(), sItem);
#else
        ScriptValueType sItem = engine->globalObject().property(pageName+"_"+item->patternName());
        if (sItem.isValid()){
            engine->newQObject(sItem, item);
        } else {
            sItem = engine->newQObject(item);
            engine->globalObject().setProperty(pageName+"_"+item->patternName(),sItem);
        }
#endif
        foreach(BaseDesignIntf* child, item->childBaseItems()){
            baseDesignIntfToScript(pageName, child);
        }
    }
}

void ScriptEngineContext::qobjectToScript(const QString& name, QObject *item)
{
    ScriptEngineType* engine = ScriptEngineManager::instance().scriptEngine();
#ifdef USE_QJSENGINE
        ScriptValueType sItem = getCppOwnedJSValue(*engine, item);
        engine->globalObject().setProperty(name, sItem);
#else
        ScriptValueType sItem = engine->globalObject().property(name);
        if (sItem.isValid()){
            engine->newQObject(sItem, item);
        } else {
            sItem = engine->newQObject(item);
            engine->globalObject().setProperty(name,sItem);
        }
#endif
}

#ifdef HAVE_UI_LOADER

#ifdef USE_QJSENGINE
void registerChildObjects(ScriptEngineType* se, ScriptValueType* sv){
    foreach(QObject* obj, sv->toQObject()->children()){
        ScriptValueType child = se->newQObject(obj);
        sv->setProperty(obj->objectName(),child);
        registerChildObjects(se, &child);
    }
}
#endif

void ScriptEngineContext::initDialogs(){
    ScriptEngineType* se = ScriptEngineManager::instance().scriptEngine();
    foreach(DialogDescriber::Ptr dialog, dialogDescribers()){
        ScriptValueType sv = se->newQObject(getDialog(dialog->name()));
#ifdef USE_QJSENGINE
        registerChildObjects(se,&sv);
#endif
        se->globalObject().setProperty(dialog->name(),sv);
    }
}

#endif


bool ScriptEngineContext::runInitScript(){

    ScriptEngineType* engine = ScriptEngineManager::instance().scriptEngine();
    ScriptEngineManager::instance().clearTableOfContens();
    ScriptEngineManager::instance().setContext(this);
    m_tableOfContens->clear();

#ifndef USE_QJSENGINE
    engine->pushContext();
#endif
    ScriptValueType res = engine->evaluate(initScript());
    if (res.isBool()) return res.toBool();
#ifdef  USE_QJSENGINE
    if (res.isError()){
        QMessageBox::critical(0,tr("Error"),
            QString("Line %1: %2 ").arg(res.property("lineNumber").toString())
                                   .arg(res.toString())
        );
        return false;
    }
#else
    if (engine->hasUncaughtException()) {
        QMessageBox::critical(0,tr("Error"),
            QString("Line %1: %2 ").arg(engine->uncaughtExceptionLineNumber())
                                   .arg(engine->uncaughtException().toString())
        );
        return false;
    }
#endif
    return true;
}

QString ScriptEngineContext::initScript() const
{
    return m_initScript;
}

void ScriptEngineContext::setInitScript(const QString& initScript)
{
    m_initScript = initScript;
}

DialogDescriber::Ptr DialogDescriber::create(const QString& name, const QByteArray& desc) {
    Ptr res(new DialogDescriber());
    res->setName(name);
    res->setDescription(desc);
    return res;
}

QString JSFunctionDesc::name() const
{
    return m_name;
}

void JSFunctionDesc::setName(const QString &name)
{
    m_name = name;
}

QString JSFunctionDesc::category() const
{
    return m_category;
}

void JSFunctionDesc::setCategory(const QString &category)
{
    m_category = category;
}

QString JSFunctionDesc::description() const
{
    return m_description;
}

void JSFunctionDesc::setDescription(const QString &description)
{
    m_description = description;
}

QString JSFunctionDesc::managerName() const
{
    return m_managerName;
}

void JSFunctionDesc::setManagerName(const QString &managerName)
{
    m_managerName = managerName;
}

QObject *JSFunctionDesc::manager() const
{
    return m_manager;
}

void JSFunctionDesc::setManager(QObject *manager)
{
    m_manager = manager;
}

QString JSFunctionDesc::scriptWrapper() const
{
    return m_scriptWrapper;
}

void JSFunctionDesc::setScriptWrapper(const QString &scriptWrapper)
{
    m_scriptWrapper = scriptWrapper;
}

QVariant ScriptFunctionsManager::calcGroupFunction(const QString &name, const QString &expressionID, const QString &bandName)
{
    if (m_scriptEngineManager->dataManager()){
        QString expression = m_scriptEngineManager->dataManager()->getExpression(expressionID);
        GroupFunction* gf =  m_scriptEngineManager->dataManager()->groupFunction(name,expression,bandName);
        if (gf){
            if (gf->isValid()){
                return gf->calculate();
            }else{
                return gf->error();
            }
        }
        else {
            return QString(QObject::tr("Function %1 not found or have wrong arguments").arg(name));
        }
    } else {
        return QString(QObject::tr("Datasource manager not found"));
    }
}

QVariant ScriptFunctionsManager::line(const QString &bandName)
{
    QString varName = QLatin1String("line_")+bandName.toLower();
    QVariant res;
    if (scriptEngineManager()->dataManager()->variable(varName).isValid()){
        res=scriptEngineManager()->dataManager()->variable(varName);
    } else res=QString("Variable line for band %1 not found").arg(bandName);
    return res;
}

QVariant ScriptFunctionsManager::numberFormat(QVariant value, const char &format, int precision, const QString& locale)
{
    return (locale.isEmpty())?QString::number(value.toDouble(),format,precision):
                              QLocale(locale).toString(value.toDouble(),format,precision);
}

QVariant ScriptFunctionsManager::dateFormat(QVariant value, const QString &format)
{
    return QLocale().toString(value.toDate(),format);
}

QVariant ScriptFunctionsManager::timeFormat(QVariant value, const QString &format)
{
    return QLocale().toString(value.toTime(),format);
}

QVariant ScriptFunctionsManager::dateTimeFormat(QVariant value, const QString &format)
{
    return QLocale().toString(value.toDateTime(),format);
}

QVariant ScriptFunctionsManager::date()
{
    return QDate::currentDate();
}

QVariant ScriptFunctionsManager::now()
{
    return QDateTime::currentDateTime();
}

QVariant ScriptFunctionsManager::currencyFormat(QVariant value, const QString &locale)
{
    QString l = (!locale.isEmpty())?locale:QLocale::system().name();
    return QLocale(l).toCurrencyString(value.toDouble());
}

QVariant ScriptFunctionsManager::currencyUSBasedFormat(QVariant value, const QString &currencySymbol)
{
    QString CurrencySymbol = (!currencySymbol.isEmpty())?currencySymbol:QLocale::system().currencySymbol();
    // Format it using USA locale
    QString vTempStr=QLocale(QLocale::English, QLocale::UnitedStates).toCurrencyString(value.toDouble());
    // Replace currency symbol if necesarry
    if (CurrencySymbol!="") vTempStr.replace("$", CurrencySymbol);
    return vTempStr;
}

void ScriptFunctionsManager::setVariable(const QString &name, QVariant value)
{
    DataSourceManager* dm = scriptEngineManager()->dataManager();
    if (dm->containsVariable(name)){
        dm->changeVariable(name,value);
    } else {
        dm->addVariable(name, value, VarDesc::User);
    }
}

QVariant ScriptFunctionsManager::getVariable(const QString &name)
{
    DataSourceManager* dm = scriptEngineManager()->dataManager();
    return dm->variable(name);
}

QVariant ScriptFunctionsManager::getField(const QString &field)
{
    DataSourceManager* dm = scriptEngineManager()->dataManager();
    return dm->fieldData(field);
}

QVariant ScriptFunctionsManager::getFieldByKeyField(const QString& datasourceName, const QString& valueFieldName, const QString& keyFieldName, QVariant keyValue)
{
    DataSourceManager* dm = scriptEngineManager()->dataManager();
    return dm->fieldDataByKey(datasourceName, valueFieldName, keyFieldName, keyValue);
}

void ScriptFunctionsManager::reopenDatasource(const QString& datasourceName)
{
    DataSourceManager* dm = scriptEngineManager()->dataManager();
    return dm->reopenDatasource(datasourceName);
}

void ScriptFunctionsManager::addTableOfContensItem(const QString& uniqKey, const QString& content, int indent)
{
    scriptEngineManager()->addTableOfContensItem(uniqKey, content, indent);
}

void ScriptFunctionsManager::clearTableOfContens()
{
    scriptEngineManager()->clearTableOfContens();
}



#ifdef USE_QJSENGINE

QFont ScriptFunctionsManager::font(const QString &family, int pointSize, bool bold, bool italic, bool underLine)
{
    QFont result (family, pointSize);
    result.setBold(bold);
    result.setItalic(italic);
    result.setUnderline(underLine);
    return result;
}

void ScriptFunctionsManager::addItemsToComboBox(QJSValue object, const QStringList &values)
{
    QComboBox* comboBox = dynamic_cast<QComboBox*>(object.toQObject());
    if (comboBox){
        comboBox->addItems(values);
    }
}

void ScriptFunctionsManager::addItemToComboBox(QJSValue object, const QString &value)
{
    QComboBox* comboBox = dynamic_cast<QComboBox*>(object.toQObject());
    if (comboBox){
        comboBox->addItem(value);
    }
}

QJSValue ScriptFunctionsManager::createComboBoxWrapper(QJSValue comboBox)
{
    QComboBox* item = dynamic_cast<QComboBox*>(comboBox.toQObject());
    if (item){
        ComboBoxWrapper* wrapper = new ComboBoxWrapper(item);
        return m_scriptEngineManager->scriptEngine()->newQObject(wrapper);
    }
    return QJSValue();
}

QJSValue ScriptFunctionsManager::createWrapper(QJSValue item)
{
    QObject* object = item.toQObject();
    if (object){
        IWrapperCreator* wrapper = m_wrappersFactory.value(object->metaObject()->className());
        if (wrapper){
            return m_scriptEngineManager->scriptEngine()->newQObject(wrapper->createWrapper(item.toQObject()));
        }
    }
    return QJSValue();
}

#endif
QFont ScriptFunctionsManager::font(QVariantMap params){
    if (!params.contains("family")){
        return QFont();
    } else {
        QFont result(params.value("family").toString());
        if (params.contains("pointSize"))
            result.setPointSize(params.value("pointSize").toInt());
        if (params.contains("bold"))
            result.setBold(params.value("bold").toBool());
        if (params.contains("italic"))
            result.setItalic(params.value("italic").toBool());
        if (params.contains("underline"))
            result.setUnderline(params.value("underline").toBool());
        return result;
    }
}

ScriptEngineManager *ScriptFunctionsManager::scriptEngineManager() const
{
    return m_scriptEngineManager;
}

void ScriptFunctionsManager::setScriptEngineManager(ScriptEngineManager *scriptEngineManager)
{
    m_scriptEngineManager = scriptEngineManager;
}

TableOfContens::~TableOfContens()
{
    clear();
}

void TableOfContens::setItem(const QString& uniqKey, const QString& content, int pageNumber, int indent)
{
    ContentItem * item = 0;
    if (m_hash.contains(uniqKey)){
        item = m_hash.value(uniqKey);
        item->content = content;
        item->pageNumber = pageNumber;
        if (indent>0)
            item->indent = indent;
    } else {
        item = new ContentItem;
        item->content = content;
        item->pageNumber = pageNumber;
        item->indent = indent;
        item->uniqKey = uniqKey;
        m_tableOfContens.append(item);
        m_hash.insert(uniqKey, item);
    }

}

void TableOfContens::slotOneSlotDS(CallbackInfo info, QVariant& data)
{
    QStringList columns;
    columns << "Content" << "Page number" << "Content Key";

    switch (info.dataType) {
        case LimeReport::CallbackInfo::RowCount:
            data = m_tableOfContens.count();
            break;
        case LimeReport::CallbackInfo::ColumnCount:
            data = columns.size();
            break;
        case LimeReport::CallbackInfo::ColumnHeaderData: {
            data = columns.at(info.index);
            break;
        }
        case LimeReport::CallbackInfo::ColumnData:
            if (info.index < m_tableOfContens.count()){
                ContentItem* item = m_tableOfContens.at(info.index);
                if (info.columnName.compare("Content",Qt::CaseInsensitive) == 0)
                    data = item->content.rightJustified(item->indent+item->content.size());
                if (info.columnName.compare("Content Key",Qt::CaseInsensitive) == 0)
                    data = item->uniqKey;
                if (info.columnName.compare("Page number",Qt::CaseInsensitive) == 0)
                    data = QString::number(item->pageNumber);
            }
            break;
        default: break;
    }
}

void LimeReport::TableOfContens::clear(){

    m_hash.clear();
    foreach(ContentItem* item, m_tableOfContens){
        delete item;
    }
    m_tableOfContens.clear();

}

#ifdef USE_QJSENGINE

QObject* ComboBoxWrapperCreator::createWrapper(QObject *item)
{
    QComboBox* comboBox = dynamic_cast<QComboBox*>(item);
    if (comboBox){
        return  new ComboBoxWrapper(comboBox);
    }
    return 0;
}

#endif

#ifndef USE_QJSENGINE
void ComboBoxPrototype::addItem(const QString &text)
{
    QComboBox* comboBox = qscriptvalue_cast<QComboBox*>(thisObject());
    if (comboBox){
        comboBox->addItem(text);
    }
}

void ComboBoxPrototype::addItems(const QStringList &texts)
{
    QComboBox* comboBox = qscriptvalue_cast<QComboBox*>(thisObject());
    if (comboBox){
        comboBox->addItems(texts);
    }
}
#endif

} //namespace LimeReport

