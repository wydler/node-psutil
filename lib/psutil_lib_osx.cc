//===========================================================================

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#include <v8.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <node.h>
#include <node_version.h>
#include <node_buffer.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

#include "psutil_lib_osx.h"

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#endif

using namespace v8;
using namespace node;

static Handle<Value> VException(const char *msg)
{
  HandleScope scope;
  return ThrowException(Exception::Error(String::New(msg)));
}

Persistent<FunctionTemplate> PSUtilLib::constructor_template;

PSUtilLib::PSUtilLib() : ObjectWrap()
{
}

void PSUtilLib::Initialize(v8::Handle<v8::Object> target)
{
  // Grab the scope of the call from Node
  HandleScope scope;

  // Define a new function template
  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("PSUtilLib"));

  // Set up the available methods
  t->InstanceTemplate()->SetInternalFieldCount(1);

  // Set up the iostat command
  NODE_SET_PROTOTYPE_METHOD(t, "iostat", PSUtilLib::IoStat);

  // Set the name of the class
  target->ForceSet(String::NewSymbol("PSUtilLib"), constructor_template->GetFunction());
}

Handle<Value> PSUtilLib::IoStat(const Arguments& args) {
  HandleScope scope;

  // Legal modes
  if(!((args.Length() == 1 && args[0]->IsFunction())
    || (args.Length() == 2 && args[0]->IsBoolean() && args[1]->IsFunction()))) {
    VException("function requires [boolea, function] or [function]");
  }

  // There's no ToFunction(), use a Cast instead.
  Local<Function> callback = Local<Function>::Cast(args[0]);

  // Create a worker object and map the information
  IoStatWorker *worker = new IoStatWorker();
  worker->error = false;
  worker->request.data = worker;
  worker->callback = Persistent<Function>::New(callback);

  // Get the value of results being returned
  worker->prDisk = args.Length() == 2 ? args[1]->ToBoolean()->BooleanValue() : false;

  // Trigger the work
  uv_queue_work(uv_default_loop(),
            &worker->request,
            PSUtilLib::Process,
            PSUtilLib::After);

  // Return the handle to the instance
  return Undefined();
}

void PSUtilLib::Process(uv_work_t* work_req) {
  // Grab the worker
  Worker *worker = static_cast<Worker*>(work_req->data);

  // Execute the worker code
  worker->execute();
}

void PSUtilLib::After(uv_work_t* work_req) {
  // Grab the scope of the call from Node
  HandleScope scope;

  Worker *worker = static_cast<Worker*>(work_req->data);

  // Set up the callback with a null first
  Local<Value> args[2] = { Local<Value>::New(Null()), Integer::New(0) };

  // Wrap the callback function call in a TryCatch so that we can call
  // node's FatalException afterwards. This makes it possible to catch
  // the exception from JavaScript land using the
  // process.on('uncaughtException') event.
  TryCatch try_catch;
  // Call the callback
  worker->callback->Call(Context::GetCurrent()->Global(), ARRAY_SIZE(args), args);
  // If we have an exception handle it as a fatalexception
  if (try_catch.HasCaught()) {
    node::FatalException(try_catch);
  }
}

// Create a new instance of BSON and passing it the existing context
Handle<Value> PSUtilLib::New(const Arguments &args)
{
  HandleScope scope;

  PSUtilLib *psUtil = new PSUtilLib();
  psUtil->Wrap(args.This());
  return args.This();
}

// Exporting function
extern "C" void init(Handle<Object> target)
{
  HandleScope scope;
  PSUtilLib::Initialize(target);
}