/*=========================================================================

  Program:   ParaView
  Module:    vtkSMProxyManagerInternals.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef __vtkSMProxyManagerInternals_h
#define __vtkSMProxyManagerInternals_h

#include "vtkCollection.h"
#include "vtkCommand.h"
#include "vtkSmartPointer.h"
#include "vtkSMCollaborationManager.h"
#include "vtkSMGlobalPropertiesManager.h"
#include "vtkSMLink.h"
#include "vtkSMMessage.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyManager.h"
#include "vtkSMProxySelectionModel.h"
#include "vtkSMLoadStateContext.h"
#include "vtkSMSession.h"
#include "vtkSMSessionClient.h"
#include "vtkSMStateLocator.h"
#include "vtkStdString.h"

#include <vtkstd/map>
#include <vtkstd/set>
#include <vtkstd/vector>
#include <vtksys/ios/sstream>
#include <vtksys/RegularExpression.hxx>
#include <vtkNew.h>

// Sub-classed to avoid symbol length explosion.
class vtkSMProxyManagerProxyInfo : public vtkObjectBase
{
public:
  vtkSmartPointer<vtkSMProxy> Proxy;
  unsigned long ModifiedObserverTag;
  unsigned long StateChangedObserverTag;
  unsigned long UpdateObserverTag;
  unsigned long UpdateInformationObserverTag;

  static vtkSMProxyManagerProxyInfo* New()
    {
    return new vtkSMProxyManagerProxyInfo();
    }

  // Description:
  // this needs to be overridden otherwise vtkDebugLeaks warnings are objects
  // are destroyed.
  void UnRegister()
    {
    int refcount = this->GetReferenceCount()-1;
    this->SetReferenceCount(refcount);
    if (refcount <= 0)
      {
#ifdef VTK_DEBUG_LEAKS
      vtkDebugLeaks::DestructClass("vtkSMProxyManagerProxyInfo");
#endif
      delete this;
      }
    }
  virtual void UnRegister(vtkObjectBase *)
    { this->UnRegister(); }

private:
  vtkSMProxyManagerProxyInfo()
    {
    this->ModifiedObserverTag = 0;
    this->StateChangedObserverTag = 0;
    this->UpdateObserverTag = 0;
    this->UpdateInformationObserverTag = 0;
    }
  ~vtkSMProxyManagerProxyInfo()
    {
    // Remove observers.
    if (this->ModifiedObserverTag && this->Proxy.GetPointer())
      {
      this->Proxy.GetPointer()->RemoveObserver(this->ModifiedObserverTag);
      this->ModifiedObserverTag = 0;
      }
    if (this->StateChangedObserverTag && this->Proxy.GetPointer())
      {
      this->Proxy.GetPointer()->RemoveObserver(this->StateChangedObserverTag);
      this->StateChangedObserverTag = 0;
      }
    if (this->UpdateObserverTag && this->Proxy.GetPointer())
      {
      this->Proxy.GetPointer()->RemoveObserver(this->UpdateObserverTag);
      this->UpdateObserverTag = 0;
      }
    if (this->UpdateInformationObserverTag && this->Proxy.GetPointer())
      {
      this->Proxy.GetPointer()->RemoveObserver(
        this->UpdateInformationObserverTag);
      this->UpdateInformationObserverTag = 0;
      }
    }
};

//-----------------------------------------------------------------------------
class vtkSMProxyManagerProxyListType :
  public vtkstd::vector<vtkSmartPointer<vtkSMProxyManagerProxyInfo> >
{
public:
  // Returns if the proxy exists in  this vector.
  bool Contains(vtkSMProxy* proxy) 
    {
    vtkSMProxyManagerProxyListType::iterator iter =
      this->begin();
    for (; iter != this->end(); ++iter)
      {
      if (iter->GetPointer()->Proxy == proxy)
        {
        return true;
        }
      }
    return false;
    }
  vtkSMProxyManagerProxyListType::iterator Find(vtkSMProxy* proxy)
    {
    vtkSMProxyManagerProxyListType::iterator iter =
      this->begin();
    for (; iter != this->end(); ++iter)
      {
      if (iter->GetPointer()->Proxy.GetPointer() == proxy)
        {
        return iter;
        }
      }
    return this->end();
    }
};

//-----------------------------------------------------------------------------
class vtkSMProxyManagerProxyMapType:
  public vtkstd::map<vtkStdString, vtkSMProxyManagerProxyListType> {};
//-----------------------------------------------------------------------------
struct vtkSMProxyManagerEntry
{
  vtkstd::string Group;
  vtkstd::string Name;
  vtkSmartPointer<vtkSMProxy> Proxy;

  vtkSMProxyManagerEntry(const char* group, const char* name, vtkSMProxy* proxy)
    {
    this->Group = group;
    this->Name = name;
    this->Proxy = proxy;
    }

  bool operator==(const vtkSMProxyManagerEntry &other) const {
    return this->Group == other.Group && this->Name == other.Name
        && this->Proxy->GetGlobalID() == other.Proxy->GetGlobalID();
  }

  bool operator<(const vtkSMProxyManagerEntry &other) const {
    if(this->Proxy->GetGlobalID() < other.Proxy->GetGlobalID())
      {
      return true;
      }
    else if( this->Proxy->GetGlobalID() == other.Proxy->GetGlobalID() &&
             this->Group == other.Group)
      {
      return this->Name < other.Name;
      }
    else if (this->Proxy->GetGlobalID() == other.Proxy->GetGlobalID())
      {
      return this->Group < other.Group;
      }
    return false;
  }

  bool operator>(const vtkSMProxyManagerEntry &other) const {
    if(this->Proxy->GetGlobalID() > other.Proxy->GetGlobalID())
      {
      return true;
      }
    else if( this->Proxy->GetGlobalID() == other.Proxy->GetGlobalID() &&
             this->Group == other.Group)
      {
      return this->Name > other.Name;
      }
    else if (this->Proxy->GetGlobalID() == other.Proxy->GetGlobalID())
      {
      return this->Group > other.Group;
      }
    return false;
  }
};
//-----------------------------------------------------------------------------
class vtkSMProxyManagerSelectionObserver : public vtkCommand
{
public:
  static vtkSMProxyManagerSelectionObserver* New()
    { return new vtkSMProxyManagerSelectionObserver(); }

  void SetTarget(vtkSMProxyManager* t)
    {
    this->Target = t;
    }

  virtual void Execute(vtkObject *obj, unsigned long event, void* data)
    {
    vtkSMMessage* msg = reinterpret_cast<vtkSMMessage*>(data);
    if(msg->HasExtension(ActiveSelectionMessage::name))
      {
      const char* name = msg->GetExtension(ActiveSelectionMessage::name).c_str();

      vtkSMProxySelectionModel* model = this->Target->GetSelectionModel(name);
      if(model)
        {
        vtkSMSession* session = this->Target->GetSession();

        int cmd = msg->GetExtension(ActiveSelectionMessage::type);
        vtkNew<vtkCollection> proxyCollection;
        int nbProxy = msg->ExtensionSize(ActiveSelectionMessage::proxy);
        for(int cc=0; cc < nbProxy; ++cc)
          {
          assert( "Invalid Proxy id" &&
                  msg->GetExtension(ActiveSelectionMessage::proxy, cc));
          proxyCollection->AddItem(
              session->GetRemoteObject(
                  msg->GetExtension(ActiveSelectionMessage::proxy, cc)));
          }

        model->Select(proxyCollection.GetPointer(), cmd);
        }
      }
    }

protected:
  vtkWeakPointer<vtkSMProxyManager> Target;
};
//-----------------------------------------------------------------------------
class vtkSMProxySelectionModelObserver : public vtkCommand
{
public:
  static vtkSMProxySelectionModelObserver* New()
    { return new vtkSMProxySelectionModelObserver(); }

  void SetTarget(vtkSMProxyManager* t)
    {
    this->Target = t;
    }

  virtual void Execute(vtkObject *obj, unsigned long event, void* data)
    {
    if (this->Target)
      {
      vtkSMSessionClient* sessionClient =
          vtkSMSessionClient::SafeDownCast(this->Target->GetSession());
      if(sessionClient)
        {
        vtkSMCollaborationManager* collaborationManager =
            sessionClient->GetCollaborationManager();
        if(collaborationManager)
          {
          vtkSMProxySelectionModel* model = vtkSMProxySelectionModel::SafeDownCast(obj);
          if(model)
            {
            vtkSMMessage selectionMessage;
            selectionMessage.SetExtension( ActiveSelectionMessage::name,
                                           this->Target->GetSelectionModelName(model));

            switch(*reinterpret_cast<int*>(data))
              {
              case vtkSMProxySelectionModel::NO_UPDATE:
                selectionMessage.SetExtension(ActiveSelectionMessage::type,
                                              ActiveSelectionMessage::NO_UPDATE);
                break;

              case vtkSMProxySelectionModel::CLEAR:
                selectionMessage.SetExtension(ActiveSelectionMessage::type,
                                              ActiveSelectionMessage::CLEAR);
                break;

              case vtkSMProxySelectionModel::SELECT:
                selectionMessage.SetExtension(ActiveSelectionMessage::type,
                                              ActiveSelectionMessage::SELECT);
                break;

              case vtkSMProxySelectionModel::DESELECT:
                selectionMessage.SetExtension(ActiveSelectionMessage::type,
                                              ActiveSelectionMessage::DESELECT);
                break;

              case vtkSMProxySelectionModel::CLEAR_AND_SELECT:
                selectionMessage.SetExtension(ActiveSelectionMessage::type,
                                              ActiveSelectionMessage::CLEAR_AND_SELECT);
                break;
                }

            unsigned int nbProxies = model->GetNumberOfSelectedProxies();
            for(unsigned int idx = 0; idx < nbProxies; idx++)
              {
              assert( "Invalid selected proxy" &&
                      model->GetSelectedProxy(idx) &&
                      model->GetSelectedProxy(idx)->GetGlobalID() != 0);
              selectionMessage.AddExtension(ActiveSelectionMessage::proxy,
                                            model->GetSelectedProxy(idx)->GetGlobalID());
              }

            collaborationManager->SendToOtherClients(&selectionMessage);
            }
          }
        }
      }
    }

protected:
  vtkWeakPointer<vtkSMProxyManager> Target;
};
//-----------------------------------------------------------------------------
struct vtkSMProxyManagerInternals
{
  // This data structure stores actual proxy instances grouped in
  // collections.
  typedef
  vtkstd::map<vtkStdString, vtkSMProxyManagerProxyMapType> ProxyGroupType;
  ProxyGroupType RegisteredProxyMap;

  // This data structure stores the tuples(group, name, proxy) to compute
  // diff when a state is loaded.
  typedef
  vtkstd::set<vtkSMProxyManagerEntry> GroupNameProxySet;
  GroupNameProxySet RegisteredProxyTuple;

  // This data structure stores a set of proxies that have been modified.
  typedef vtkstd::set<vtkSMProxy*> SetOfProxies;
  SetOfProxies ModifiedProxies;

  // Data structure to save registered links.
  typedef vtkstd::map<vtkStdString, vtkSmartPointer<vtkSMLink> >
    LinkType;
  LinkType RegisteredLinkMap;

  // Data structure for selection models.
  typedef vtkstd::map<vtkstd::string, vtkSmartPointer<vtkSMProxySelectionModel> >
    SelectionModelsType;
  SelectionModelsType SelectionModels;

  // Data structure for selection models.
  typedef vtkstd::map<vtkstd::string, vtkSmartPointer<vtkSMProxySelectionModelObserver> >
    SelectionModelObserversType;
  SelectionModelObserversType SelectionModelObservers;

  // Data structure for storing GlobalPropertiesManagers.
  typedef vtkstd::map<vtkstd::string,
          vtkSmartPointer<vtkSMGlobalPropertiesManager> >
            GlobalPropertiesManagersType;
  typedef vtkstd::map<vtkstd::string,
          unsigned long >
            GlobalPropertiesManagersCallBackIDType;
  GlobalPropertiesManagersType GlobalPropertiesManagers;
  GlobalPropertiesManagersCallBackIDType GlobalPropertiesManagersCallBackID;

  // Data structure for storing the fullState
  vtkSMMessage State;

  // Keep ref to the proxyManager to access the session
  vtkSMProxyManager *ProxyManager;

  // Keep ref to the proxyManager collaboration observer
  vtkNew<vtkSMProxyManagerSelectionObserver> CollaborationObserver;

  // Helper methods -----------------------------------------------------------
  void FindProxyTuples(vtkSMProxy* proxy,
                       vtkstd::set<vtkSMProxyManagerEntry> &tuplesFounds)
    {
    vtkstd::set<vtkSMProxyManagerEntry>::iterator iter;
    iter = this->RegisteredProxyTuple.begin();
    while(iter != this->RegisteredProxyTuple.end())
      {
      if(iter->Proxy == proxy)
        {
        tuplesFounds.insert(*iter);
        }
      iter++;
      }
    }

  // --------------------------------------------------------------------------
  void UpdateOnly( vtkSMStateLocator* locator, vtkCollection* proxyToUpdate)
    {
    int size = proxyToUpdate->GetNumberOfItems();
    vtkNew<vtkSMLoadStateContext> ctx;
    ctx->SetRequestOrigin(vtkSMLoadStateContext::UNDEFINED);
    vtkSMMessage proxyState;
    for(int i=0; i < size; i++)
      {
      vtkSMProxy* proxy = vtkSMProxy::SafeDownCast(proxyToUpdate->GetItemAsObject(i));
      if(locator && locator->FindState(proxy->GetGlobalID(), &proxyState))
        {
        proxy->LoadState(&proxyState, locator, ctx.GetPointer());
        proxy->UpdateVTKObjects();
        }
      }
    }

  // --------------------------------------------------------------------------
  void ExtractProxyInvolved(const vtkSMMessage* msg,
                            vtkstd::set<vtkTypeUInt32>& globalIdList,
                            vtkSMStateLocator* locator)
    {
    vtkstd::set<vtkTypeUInt32> localDiscovery;
    int idx, size;

    // Manage ProxyManager registration informations
    size = msg->ExtensionSize(ProxyManagerState::registered_proxy);
    for(idx=0; idx < size; ++idx)
      {
      localDiscovery.insert(msg->GetExtension(ProxyManagerState::registered_proxy, idx).global_id());
      }
    // Manage SubProxy informations
    size = msg->ExtensionSize(ProxyState::subproxy);
    for(idx=0; idx < size; ++idx)
      {
      localDiscovery.insert(msg->GetExtension(ProxyState::subproxy, idx).global_id());
      }
    // Manage Property informations
    size = msg->ExtensionSize(ProxyState::property);
    for(idx=0; idx < size; ++idx)
      {
      const ProxyState_Property *prop = &msg->GetExtension(ProxyState::property, idx);
      const Variant *value = &prop->value();
      int nbProxies = value->proxy_global_id_size();
      for(int i=0; i < nbProxies; ++i)
        {
        localDiscovery.insert(value->proxy_global_id(i));
        }
      }

    // Remove local proxy that have been examine
    vtkstd::set<vtkTypeUInt32>::iterator iter;
    for(iter = globalIdList.begin(); iter != globalIdList.end(); iter++)
      {
      localDiscovery.erase(*iter);
      }

    // Add local discovery to global and inspect local ones
    globalIdList.insert(localDiscovery.begin(),localDiscovery.end());
    if(locator)
      {
      vtkSMMessage tmp;
      for(iter = localDiscovery.begin(); iter != localDiscovery.end(); iter++)
        {
        if(locator->FindState(*iter, &tmp))
          {
          this->ExtractProxyInvolved(&tmp, globalIdList, locator);
          }
        }
      }
    }

  // --------------------------------------------------------------------------
  void CreateOnly( vtkstd::set<vtkTypeUInt32>& globalIdList, vtkSMStateLocator* locator,
                   vtkCollection* createdProxyHolder)
    {
    vtkstd::set<vtkTypeUInt32>::iterator iter = globalIdList.begin();
    for(; iter != globalIdList.end(); iter++)
      {
      vtkTypeUInt32 globalId = *iter;
      vtkSMProxy *proxy =
          vtkSMProxy::SafeDownCast(
              this->ProxyManager->GetSession()->GetRemoteObject(globalId));

      if(!proxy)
        {
        vtkSMMessage proxyState;
        if(locator && locator->FindState(globalId, &proxyState))
          {
          vtkNew<vtkSMLoadStateContext> ctx;
          ctx->SetRequestOrigin(vtkSMLoadStateContext::RE_NEW_PROXY);
          ctx->SetLoadDefinitionOnly(true); // CreateOnly
          vtkSMProxy* reNewProxy = this->ProxyManager->NewProxy( &proxyState, locator, ctx.GetPointer());
          if(reNewProxy)
            {
            createdProxyHolder->AddItem(reNewProxy);
            cout << "Create: " << reNewProxy->GetClassName() << " " << reNewProxy->GetGlobalIDAsString() << endl;
            reNewProxy->FastDelete();
            }
          }
        }
      }
    }

  // --------------------------------------------------------------------------
  void ComputeDelta(const vtkSMMessage* newState,
                    vtkSMStateLocator* locator,
                    vtkstd::set<vtkSMProxyManagerEntry> &toRegister,
                    vtkstd::set<vtkSMProxyManagerEntry> &toUnregister)
    {
    // Fill equivalent temporary data structure
    vtkstd::set<vtkSMProxyManagerEntry> newStateContent;
    int max = newState->ExtensionSize(ProxyManagerState::registered_proxy);
    for(int cc=0; cc < max; cc++)
      {
      ProxyManagerState_ProxyRegistrationInfo reg =
          newState->GetExtension(ProxyManagerState::registered_proxy, cc);
      vtkSMProxy *proxy =
          vtkSMProxy::SafeDownCast(
              this->ProxyManager->GetSession()->GetRemoteObject(reg.global_id()));

      if(!proxy)
        {
        vtkSMProxy *reNewProxy = this->ProxyManager->ReNewProxy(reg.global_id(), locator);
        if(reNewProxy)
          {
          newStateContent.insert(vtkSMProxyManagerEntry(reg.group().c_str(), reg.name().c_str(), reNewProxy));
          reNewProxy->Delete();
          }
        }
      else
        {
        newStateContent.insert(vtkSMProxyManagerEntry(reg.group().c_str(), reg.name().c_str(), proxy));
        }
      }

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // FIXME there might be a better way to do that
    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    // Look for proxy to Register
    vtkstd::set<vtkSMProxyManagerEntry>::iterator iter;
    iter = newStateContent.begin();
    while(iter != newStateContent.end())
      {
      if(this->RegisteredProxyTuple.find(*iter) == this->RegisteredProxyTuple.end())
        {
        toRegister.insert(*iter);
        cout << "+++ Register " << iter->Group.c_str() << " - " << iter->Name.c_str() << endl;
        }
      iter++;
      }

    // Look for proxy to Unregister
    iter = this->RegisteredProxyTuple.begin();
    vtksys::RegularExpression prototypesRe("_prototypes$");
    while(iter != this->RegisteredProxyTuple.end())
      {
      if( newStateContent.find(*iter) == newStateContent.end() &&
          !prototypesRe.find(iter->Group.c_str()))
        {
        toUnregister.insert(*iter);
        cout << "--- UnRegister " << iter->Group.c_str() << " - " << iter->Name.c_str() << endl;
        }
      iter++;
      }
    }

  // --------------------------------------------------------------------------
  void RemoveTuples( const char* name,
                     vtkstd::set<vtkSMProxyManagerEntry> &removedEntries)
    {
    // Convert param
    vtkstd::string nameString = name;

    // Deal with set
    GroupNameProxySet resultSet;
    vtkstd::set<vtkSMProxyManagerEntry>::iterator iter;
    iter = this->RegisteredProxyTuple.begin();
    while(iter != this->RegisteredProxyTuple.end())
      {
      if(iter->Name != nameString)
        {
        resultSet.insert(*iter);
        }
      iter++;
      }
    this->RegisteredProxyTuple = resultSet;

    // Deal with map only (true)
    vtkSMProxyManagerInternals::ProxyGroupType::iterator it =
        this->RegisteredProxyMap.begin();
    for (; it != this->RegisteredProxyMap.end(); it++)
      {
      vtkSMProxyManagerProxyMapType::iterator it2 =
        it->second.find(name);
      if (it2 != it->second.end())
        {
        this->RemoveTuples(it->first.c_str(), name, removedEntries, true);
        }
      }

    // Deal with State
    vtkSMMessage backup;
    backup.CopyFrom(this->State);
    int nbRegisteredProxy = this->State.ExtensionSize(ProxyManagerState::registered_proxy);
    this->State.ClearExtension(ProxyManagerState::registered_proxy);
    for(int cc=0; cc < nbRegisteredProxy; ++cc)
      {
      const ProxyManagerState_ProxyRegistrationInfo *reg =
          &backup.GetExtension(ProxyManagerState::registered_proxy, cc);

      if( reg->name() != nameString ) // Keep it
        {
        this->State.AddExtension(ProxyManagerState::registered_proxy)->CopyFrom(*reg);
        }
      }
    }

  // --------------------------------------------------------------------------
  void RemoveTuples(const char* group, const char* name,
                    vtkstd::set<vtkSMProxyManagerEntry> &removedEntries,
                    bool doMapOnly)
    {
    // Convert parameters
    vtkstd::string groupString = group;
    vtkstd::string nameString = name;

    // Deal with set
    if(!doMapOnly)
      {
      GroupNameProxySet resultSet;
      vtkstd::set<vtkSMProxyManagerEntry>::iterator iter;
      iter = this->RegisteredProxyTuple.begin();
      while(iter != this->RegisteredProxyTuple.end())
        {
        if(iter->Group != groupString || iter->Name != nameString)
          {
          resultSet.insert(*iter);
          }
        iter++;
        }
      this->RegisteredProxyTuple = resultSet;
      }

    // Deal with map
    vtkSMProxyManagerInternals::ProxyGroupType::iterator it =
        this->RegisteredProxyMap.find(group);
    if ( it != this->RegisteredProxyMap.end() )
      {
      vtkSMProxyManagerProxyMapType::iterator it2 = it->second.find(name);
      if (it2 != it->second.end())
        {
        vtkSMProxyManagerProxyListType::iterator it3 = it2->second.begin();
        while(it3 != it2->second.end())
          {
          removedEntries.insert(vtkSMProxyManagerEntry( group, name,
                                                        (*it3)->Proxy));
          it3++;
          }
        it->second.erase(it2);
        }
      }

    // Deal with state
    vtksys::RegularExpression prototypesRe("_prototypes$");
    if(!doMapOnly && !prototypesRe.find(group))
      {
      vtkSMMessage backup;
      backup.CopyFrom(this->State);
      int nbRegisteredProxy =
          this->State.ExtensionSize(ProxyManagerState::registered_proxy);
      this->State.ClearExtension(ProxyManagerState::registered_proxy);
      for(int cc=0; cc < nbRegisteredProxy; ++cc)
        {
        const ProxyManagerState_ProxyRegistrationInfo *reg =
            &backup.GetExtension(ProxyManagerState::registered_proxy, cc);

        if( reg->group() != groupString || reg->name() != nameString ) // Keep it
          {
          this->State.AddExtension(ProxyManagerState::registered_proxy)->CopyFrom(*reg);
          }
        }
      }
    }

  // --------------------------------------------------------------------------
  // Return true if the given tuple has been found
  bool RemoveTuples(const char* group, const char* name, vtkSMProxy* proxy)
    {
    // Convert parameters
    vtkstd::string groupString = group;
    vtkstd::string nameString = name;

    // Will be overriden if the proxy is found
    bool found = false;

    // Deal with set
    this->RegisteredProxyTuple.erase(vtkSMProxyManagerEntry(group,name,proxy));

    // Deal with map
    vtkSMProxyManagerInternals::ProxyGroupType::iterator it =
        this->RegisteredProxyMap.find(group);
    if ( it != this->RegisteredProxyMap.end() )
      {
      vtkSMProxyManagerProxyMapType::iterator it2 = it->second.find(name);
      if (it2 != it->second.end())
        {
        vtkSMProxyManagerProxyListType::iterator it3 = it2->second.Find(proxy);
        if (it3 != it2->second.end())
          {
          found = true;
          it2->second.erase(it3);
          }
        if (it2->second.size() == 0)
          {
          it->second.erase(it2);
          }
        }
      }

    // Deal with state
    vtksys::RegularExpression prototypesRe("_prototypes$");
    if (!prototypesRe.find(group))
      {
      vtkSMMessage backup;
      backup.CopyFrom(this->State);

      int nbRegisteredProxy = this->State.ExtensionSize(ProxyManagerState::registered_proxy);
      this->State.ClearExtension(ProxyManagerState::registered_proxy);


      for(int cc=0; cc < nbRegisteredProxy; ++cc)
        {
        const ProxyManagerState_ProxyRegistrationInfo *reg =
            &backup.GetExtension(ProxyManagerState::registered_proxy, cc);

        if( reg->group() ==  groupString && reg->name() == nameString
            && reg->global_id() == proxy->GetGlobalID() )
          {
          // Do not keep it
          }
        else
          {
          // Keep it
          this->State.AddExtension(ProxyManagerState::registered_proxy)->CopyFrom(*reg);
          }
        }
      }

    return found;
    }
};

#endif

