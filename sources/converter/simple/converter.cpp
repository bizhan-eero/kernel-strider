/* Convert trace generated by the KEDR into simple human-readable messages */

#include <kedr/kedr_trace_reader/kedr_trace_reader.h>

#include <kedr/object_types.h> /* Enumerations describing events */

#include <iostream>
#include <stdexcept>

#include <iomanip>

#include <unistd.h> /* sleep */

#include <fstream>

/* Wrapper around addresses for pretty printing */
template<class T>
struct Addr;

template<>
struct Addr<uint32_t>
{
    Addr(const CTFVarInt& var, CTFContext& context):
        addr(var.getUInt32(context)) {}
    
    uint32_t addr;
};

template<>
struct Addr<uint64_t>
{
    Addr(const CTFVarInt& var, CTFContext& context):
        addr(var.getUInt64(context)) {}
    
    uint64_t addr;
};


template<class T>
std::ostream& operator<<(std::ostream& os, const struct Addr<T>& addr)
{
    std::ios_base::fmtflags oldFlags = os.flags();
    char oldFill = os.fill();
    int oldWidth = os.width();
    
    os.fill('0');
    os << "0x" << std::hex << std::setw(2 * sizeof(T)) << std::right
        << addr.addr;
    
    os.flags(oldFlags);
    os.fill(oldFill);
    os.width(oldWidth);
    
    return os;
}

/* Helpers for search typed variables */
static const CTFVarInt& findInt(const CTFReader& reader, const std::string& name)
{
    const CTFVar* var = reader.findVar(name);
    if(var == NULL)
    {
        std::cerr << "Failed to find integer variable '" << name << "'.\n";
        throw std::invalid_argument("Invalid variable name");
    }
    if(!var->isInt())
    {
        std::cerr << "Variable with name '" << name << "' is not integer.\n";
        throw std::invalid_argument("Invalid variable type");
    }
    return *static_cast<const CTFVarInt*>(var);
}

static const CTFVarEnum& findEnum(const CTFReader& reader, const std::string& name)
{
    const CTFVar* var = reader.findVar(name);
    if(var == NULL)
    {
        std::cerr << "Failed to find enumeration variable '" << name << "'.\n";
        throw std::invalid_argument("Invalid variable name");
    }
    if(!var->isEnum())
    {
        std::cerr << "Variable with name '" << name << "' is not enumeration.\n";
        throw std::invalid_argument("Invalid variable type");
    }
    return *static_cast<const CTFVarEnum*>(var);
}

static const CTFVarArray& findArray(const CTFReader& reader, const std::string& name)
{
    const CTFVar* var = reader.findVar(name);
    if(var == NULL)
    {
        std::cerr << "Failed to find array-like variable '" << name << "'.\n";
        throw std::invalid_argument("Invalid variable name");
    }
    if(!var->isArray())
    {
        std::cerr << "Variable with name '" << name << "' is not array-like.\n";
        throw std::invalid_argument("Invalid variable type");
    }
    return *static_cast<const CTFVarArray*>(var);
}

class EventPrinterType;
/* Output information about event in human-readable form */
class EventPrinter
{
public:
    EventPrinter(const KEDRTraceReader& traceReader);
    ~EventPrinter(void);
    
    void print(CTFReader::Event& event, std::ostream& os) const;

    /* Helper for printers for concrete type of events */
    template<class T>
    void printHeader(CTFReader::Event& event, std::ostream& os) const;
    
    const KEDRTraceReader& getTraceReader(void) const
        {return traceReader;}
private:
    const KEDRTraceReader& traceReader;
    /* Printers for each index of event type enumeration. */
    std::vector<EventPrinterType*> typePrinters;
    
    template<class T>
    void setupTypePrinters(void);
    
    const CTFVarEnum& eventTypeVar;
    /* Variable used in header printing */
    const CTFVarInt& cpuVar;
    const CTFVarInt& timestampVar;
    const CTFVarInt& tidVar;
};

EventPrinter::EventPrinter(const KEDRTraceReader& traceReader)
    : traceReader(traceReader),
    eventTypeVar(findEnum(traceReader, "stream.event.header.type")),
    cpuVar(findInt(traceReader, "trace.packet.header.cpu")),
    timestampVar(findInt(traceReader, "stream.event.context.timestamp")),
    tidVar(findInt(traceReader, "stream.event.context.tid"))
{
    const std::string* pointerBits = traceReader.findParameter("trace.pointer_bits");
    if(!pointerBits)
    {
        std::cerr << "Unknown number bits in pointer for trace.\n";
        throw std::logic_error("Incorrect trace.");
    }
    if(*pointerBits == "32")
    {
        setupTypePrinters<uint32_t>();
    }
    else if(*pointerBits == "64")
    {
        setupTypePrinters<uint64_t>();
    }
    else
    {
        std::cerr << "Unknown number bits in pointer for trace:"
            << *pointerBits << ".\n";
        throw std::logic_error("Incorrect trace.");
    }
}

template<class T>
void EventPrinter::printHeader(CTFReader::Event& event, std::ostream& os) const
{
    std::ios_base::fmtflags oldFlags = os.flags();
    char oldFill = os.fill();
    int oldWidth = os.width();
    
    int cpu = cpuVar.getInt32(event);
    uint64_t timestamp = timestampVar.getUInt64(event);
    uint64_t seconds = timestamp / 1000000000L;
    uint64_t ns = timestamp % 1000000000L;
    
    os << Addr<T>(tidVar, event);
    
    os << "\t";
    
    os.fill('0');
    os << "[" << std::dec << std::setw(3) << cpu << "]";
    
    os << "\t";

    os.fill('0');    
    os << std::dec << seconds  << "." << std::setw(6) << ns/1000;
    
    os << ":\t";
    
    os.flags(oldFlags);
    os.fill(oldFill);
    os.width(oldWidth);
}

/* Abstract class which process event of concrete type */
class EventPrinterType
{
public:
    virtual ~EventPrinterType() {}
    
    virtual void print(CTFReader::Event& event, std::ostream& os) = 0;
};

/* Concrete specializations of the class */

/* Print event in one line. This approach is used for the most event types. */
template<class T>
class EventPrinterOneLine : public EventPrinterType
{
public:
    EventPrinterOneLine(const EventPrinter& eventPrinter)
        : eventPrinter(eventPrinter) {}
    
    void print(CTFReader::Event& event, std::ostream& os);
protected:
    /* 
     * Print only type-specific event fields.
     * 
     * Following stream properties will be restored automatically
     * after function call:
     * 
     * - format flags
     * - width
     * - fill character
     */
    virtual void printEventFields(CTFReader::Event& event, std::ostream& os) = 0;
private:
    const EventPrinter& eventPrinter;
};

template<class T>
void EventPrinterOneLine<T>::print(CTFReader::Event& event, std::ostream& os)
{
    eventPrinter.printHeader<T>(event, os);
    
    std::ios_base::fmtflags oldFlags = os.flags();
    char oldFill = os.fill();
    int oldWidth = os.width();
    
    printEventFields(event, os);
    
    os.flags(oldFlags);
    os.fill(oldFill);
    os.width(oldWidth);
    
    os << std::endl;
}

/* Unknown event type (for intermediate implementation). */
template<class T>
class EventPrinterUnknown: public EventPrinterOneLine<T>
{
public:
    EventPrinterUnknown(const EventPrinter& eventPrinter)
        : EventPrinterOneLine<T>(eventPrinter),
        eventTypeVar(findEnum(eventPrinter.getTraceReader(),
            "stream.event.header.type"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        std::string type = eventTypeVar.getEnum(event);
        if(type == "") type = "(unknown)";
        
        os << "Unknown event type: " << type;
    }
private:
    const CTFVarEnum& eventTypeVar;
};

/* Function entry/exit(common data - common base class) */
template<class T>
class EventPrinterFEE: public EventPrinterOneLine<T>
{
public:
    EventPrinterFEE(const EventPrinter& eventPrinter,
        const std::string& eventType, const std::string& what):
        EventPrinterOneLine<T>(eventPrinter), what(what),
        funcAddrVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".func")){}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        os << what << " " << Addr<T>(funcAddrVar, event);
    }
private:
    const std::string what;
    const CTFVarInt& funcAddrVar;
};

/* Function entry */
template<class T>
class EventPrinterFEntry: public EventPrinterFEE<T>
{
public:
    EventPrinterFEntry(const EventPrinter& eventPrinter)
        : EventPrinterFEE<T>(eventPrinter, "fentry", "Enter to function") {}
};

/* Function exit */
template<class T>
class EventPrinterFExit: public EventPrinterFEE<T>
{
public:
    EventPrinterFExit(const EventPrinter& eventPrinter)
        : EventPrinterFEE<T>(eventPrinter, "fexit", "Exit from function") {}
};

/* Memory accesses */
template<class T>
class EventPrinterMA: public EventPrinterType
{
public:
    EventPrinterMA(const EventPrinter& eventPrinter):
        eventPrinter(eventPrinter),
        accessesVar(findArray(eventPrinter.getTraceReader(), "event.fields.ma")),
        pcVar(findInt(eventPrinter.getTraceReader(), "event.fields.ma[].pc")),
        addrVar(findInt(eventPrinter.getTraceReader(), "event.fields.ma[].addr")),
        sizeVar(findInt(eventPrinter.getTraceReader(), "event.fields.ma[].size")),
        accessTypeVar(findInt(eventPrinter.getTraceReader(), "event.fields.ma[].access_type"))
        {}
    
    void print(CTFReader::Event& event, std::ostream& os);
private:
    const EventPrinter& eventPrinter;
    
    const CTFVarArray& accessesVar;
    
    const CTFVarInt& pcVar;
    const CTFVarInt& addrVar;
    const CTFVarInt& sizeVar;
    const CTFVarInt& accessTypeVar;
};

template<class T>
void EventPrinterMA<T>::print(CTFReader::Event& event, std::ostream& os)
{
    for(CTFVarArray::ElemIterator iter(accessesVar, event);
        iter;
        ++iter)
    {
        eventPrinter.printHeader<T>(event, os);
        
        Addr<T> addr(addrVar, *iter);
        uint64_t size = sizeVar.getUInt64(*iter);
        int accessType = accessTypeVar.getInt32(*iter);
        
        os << "At " << Addr<T>(pcVar, *iter) << "\t";
        switch(accessType)
        {
        case KEDR_ET_MREAD:
            os << "Read " << size << " bytes from " << addr;
        break;
        case KEDR_ET_MWRITE:
            os << "Write " << size << " bytes to " << addr;
        break;
        case KEDR_ET_MUPDATE:
            os << "Update " << size << " bytes at " << addr;
        break;
        default:
            os << "Unknown memory access operation";
        }
        
        os << "\n";
    }
}

/* Function call pre\post events(common data - common base class) */
template<class T>
class EventPrinterFC: public EventPrinterOneLine<T>
{
public:
    EventPrinterFC(const EventPrinter& eventPrinter,
        const std::string& eventType, const std::string& what):
        EventPrinterOneLine<T>(eventPrinter), what(what),
        funcAddrVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".func")),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".pc"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        os << what << " " << Addr<T>(funcAddrVar, event)
            << " at " << Addr<T>(pcVar, event);
    }
private:
    const std::string what;
    const CTFVarInt& funcAddrVar;
    const CTFVarInt& pcVar;
};

/* Function call pre */
template<class T>
class EventPrinterFCPre: public EventPrinterFC<T>
{
public:
    EventPrinterFCPre(const EventPrinter& eventPrinter):
        EventPrinterFC<T>(eventPrinter, "fcpre", "Before call of function") {}
};

/* Function call post */
template<class T>
class EventPrinterFCPost: public EventPrinterFC<T>
{
public:
    EventPrinterFCPost(const EventPrinter& eventPrinter):
        EventPrinterFC<T>(eventPrinter, "fcpost", "After call of function") {}
};


/* Locked operations events(common data - common base class) */
template<class T>
class EventPrinterLockedOp: public EventPrinterOneLine<T>
{
public:
    EventPrinterLockedOp(const EventPrinter& eventPrinter,
        const std::string& eventType, const std::string& what):
        EventPrinterOneLine<T>(eventPrinter), what(what),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".pc")),
        addrVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".addr")),
        sizeVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".size"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        uint64_t size = sizeVar.getUInt64(event);
        
        os << "Locked " << what << " " << Addr<T>(addrVar, event)
            << " of size " << size << " at " <<  Addr<T>(pcVar, event);
    }
private:
    const std::string what;
    const CTFVarInt& pcVar;
    const CTFVarInt& addrVar;
    const CTFVarInt& sizeVar;
};


/* Locked update */
template<class T>
class EventPrinterLockedUpdate: public EventPrinterLockedOp<T>
{
public:
    EventPrinterLockedUpdate(const EventPrinter& eventPrinter)
        : EventPrinterLockedOp<T>(eventPrinter, "lma_update", "update of") {}
};

/* Locked read */
template<class T>
class EventPrinterLockedRead: public EventPrinterLockedOp<T>
{
public:
    EventPrinterLockedRead(const EventPrinter& eventPrinter)
        : EventPrinterLockedOp<T>(eventPrinter, "lma_read", "read from") {}
};

/* Locked write */
template<class T>
class EventPrinterLockedWrite: public EventPrinterLockedOp<T>
{
public:
    EventPrinterLockedWrite(const EventPrinter& eventPrinter)
        : EventPrinterLockedOp<T>(eventPrinter, "lma_write", "write to") {}
};

/* I/O operation */
template<class T>
class EventPrinterIO: public EventPrinterOneLine<T>
{
public:
    EventPrinterIO(const EventPrinter& eventPrinter):
        EventPrinterOneLine<T>(eventPrinter),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.ioma.pc")),
        addrVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.ioma.addr")),
        sizeVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.ioma.size")),
        accessTypeVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.ioma.access_type"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        uint64_t size = sizeVar.getUInt64(event);
        uint8_t accessType = accessTypeVar.getUInt32(event);
        
        std::string what;
        switch(accessType)
        {
        case KEDR_ET_MREAD:
            what = "read from";
        break;
        case KEDR_ET_MWRITE:
            what = "write to";
        break;
        case KEDR_ET_MUPDATE:
            what = "update of";
        break;
        }
        
        os << "I/O " << what << " " << Addr<T>(addrVar, event)
            << " of size " << size << " at " << Addr<T>(pcVar, event);
    }
private:
    const CTFVarInt& pcVar;
    const CTFVarInt& addrVar;
    const CTFVarInt& sizeVar;
    const CTFVarInt& accessTypeVar;
};

/* Memory barriers(common data - common base class) */
template<class T>
class EventPrinterMB: public EventPrinterOneLine<T>
{
public:
    EventPrinterMB(const EventPrinter& eventPrinter,
        const std::string& eventType, const std::string& what):
        EventPrinterOneLine<T>(eventPrinter), what(what),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".pc"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        os << what << " at " <<  Addr<T>(pcVar, event);
    }
private:
    const std::string what;
    const CTFVarInt& pcVar;
};

/* Memory write barrier */
template<class T>
class EventPrinterMWB: public EventPrinterMB<T>
{
public:
    EventPrinterMWB(const EventPrinter& eventPrinter):
        EventPrinterMB<T>(eventPrinter, "mwb", "Memory write barrier") {}
};

/* Memory read barrier */
template<class T>
class EventPrinterMRB: public EventPrinterMB<T>
{
public:
    EventPrinterMRB(const EventPrinter& eventPrinter):
        EventPrinterMB<T>(eventPrinter, "mrb", "Memory read barrier") {}
};

/* Memory full barrier */
template<class T>
class EventPrinterMFB: public EventPrinterMB<T>
{
public:
    EventPrinterMFB(const EventPrinter& eventPrinter):
        EventPrinterMB<T>(eventPrinter, "mfb", "Full memory barrier") {}
};

/* Alloc */
template<class T>
class EventPrinterAlloc: public EventPrinterOneLine<T>
{
public:
    EventPrinterAlloc(const EventPrinter& eventPrinter):
        EventPrinterOneLine<T>(eventPrinter),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.alloc.pc")),
        sizeVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.alloc.size")),
        pointerVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.alloc.pointer"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        uint64_t size = sizeVar.getUInt64(event);
        
        os << "Allocation of " << size << " bytes " << " at "
            <<  Addr<T>(pcVar, event)
            << " (pointer returned is " << Addr<T>(pointerVar, event)
            <<")";
    }
private:
    const CTFVarInt& pcVar;
    const CTFVarInt& sizeVar;
    const CTFVarInt& pointerVar;
};

/* Free */
template<class T>
class EventPrinterFree: public EventPrinterOneLine<T>
{
public:
    EventPrinterFree(const EventPrinter& eventPrinter):
        EventPrinterOneLine<T>(eventPrinter),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.free.pc")),
        pointerVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.free.pointer"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        os << "Freeing of pointer " << Addr<T>(pointerVar, event) << " at "
            << Addr<T>(pointerVar, event);
    }
private:
    const CTFVarInt& pcVar;
    const CTFVarInt& pointerVar;
};

/* Lock events(common data - common base class) */
template<class T>
class EventPrinterLockOp: public EventPrinterOneLine<T>
{
public:
    EventPrinterLockOp(const EventPrinter& eventPrinter,
        const std::string& eventType):
        EventPrinterOneLine<T>(eventPrinter),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".pc")),
        objectVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".object")),
        lockTypeVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".type"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        os << what(event) << " at " <<  Addr<T>(pcVar, event) 
            << " (object is " << Addr<T>(objectVar, event) << ")";
    }
    
    virtual std::string what(CTFReader::Event& event) = 0;

    enum kedr_lock_type getLockType(CTFReader::Event& event)
    {
        return (enum kedr_lock_type)lockTypeVar.getUInt32(event);
    }
private:
    const CTFVarInt& pcVar;
    const CTFVarInt& objectVar;
    const CTFVarInt& lockTypeVar;
};

template<class T>
class EventPrinterLock: public EventPrinterLockOp<T>
{
public:
    EventPrinterLock(const EventPrinter& eventPrinter):
        EventPrinterLockOp<T>(eventPrinter, "lock"){}
protected:
    std::string what(CTFReader::Event& event)
    {
        switch(EventPrinterLockOp<T>::getLockType(event))
        {
        case KEDR_LT_MUTEX:
            return "Mutex lock";
        break;
        case KEDR_LT_SPINLOCK:
            return "Spinlock lock";
        break;
        case KEDR_LT_WLOCK:
            return "Spinlock writer lock";
        break;
        default:
            return "Unknown lock operation";
        }
    }
};

template<class T>
class EventPrinterUnlock: public EventPrinterLockOp<T>
{
public:
    EventPrinterUnlock(const EventPrinter& eventPrinter):
        EventPrinterLockOp<T>(eventPrinter, "unlock"){}
protected:
    std::string what(CTFReader::Event& event)
    {
        switch(EventPrinterLockOp<T>::getLockType(event))
        {
        case KEDR_LT_MUTEX:
            return "Mutex unlock";
        break;
        case KEDR_LT_SPINLOCK:
            return "Spinlock unlock";
        break;
        case KEDR_LT_WLOCK:
            return "Spinlock writer unlock";
        break;
        default:
            return "Unknown unlock operation";
        }
    }
};

template<class T>
class EventPrinterRLock: public EventPrinterLockOp<T>
{
public:
    EventPrinterRLock(const EventPrinter& eventPrinter):
        EventPrinterLockOp<T>(eventPrinter, "rlock"){}
protected:
    std::string what(CTFReader::Event& event)
    {
        switch(EventPrinterLockOp<T>::getLockType(event))
        {
        case KEDR_LT_RLOCK:
            return "Spinlock reader lock";
        break;
        default:
            return "Unknown reader lock operation";
        }
    }
};

template<class T>
class EventPrinterRUnlock: public EventPrinterLockOp<T>
{
public:
    EventPrinterRUnlock(const EventPrinter& eventPrinter):
        EventPrinterLockOp<T>(eventPrinter, "runlock"){}
protected:
    std::string what(CTFReader::Event& event)
    {
        switch(EventPrinterLockOp<T>::getLockType(event))
        {
        case KEDR_LT_RLOCK:
            return "Spinlock reader unlock";
        break;
        default:
            return "Unknown reader unlock operation";
        }
    }
};

/* Signal/wait events(common data - common base class) */
template<class T>
class EventPrinterSW: public EventPrinterOneLine<T>
{
public:
    EventPrinterSW(const EventPrinter& eventPrinter,
        const std::string& eventType):
        EventPrinterOneLine<T>(eventPrinter),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".pc")),
        objectVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".object")),
        waitTypeVar(findInt(eventPrinter.getTraceReader(),
            "event.fields." + eventType + ".type"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        os << what(event) << " at " <<  Addr<T>(pcVar, event) 
            << " (object is " << Addr<T>(objectVar, event) << ")";
    }
    
    virtual std::string what(CTFReader::Event& event) = 0;

    enum kedr_sw_object_type getWaitType(CTFReader::Event& event)
    {
        return (enum kedr_sw_object_type)waitTypeVar.getUInt32(event);
    }
private:
    const CTFVarInt& pcVar;
    const CTFVarInt& objectVar;
    const CTFVarInt& waitTypeVar;
};


template<class T>
class EventPrinterSignal: public EventPrinterSW<T>
{
public:
    EventPrinterSignal(const EventPrinter& eventPrinter):
        EventPrinterSW<T>(eventPrinter, "signal"){}
protected:
    std::string what(CTFReader::Event& event)
    {
        switch(EventPrinterSW<T>::getWaitType(event))
        {
        case KEDR_SWT_COMMON:
            return "Common signal";
        break;
        default:
            return "Unknown signal";
        }
    }
};

template<class T>
class EventPrinterWait: public EventPrinterSW<T>
{
public:
    EventPrinterWait(const EventPrinter& eventPrinter):
        EventPrinterSW<T>(eventPrinter, "signal"){}
protected:
    std::string what(CTFReader::Event& event)
    {
        switch(EventPrinterSW<T>::getWaitType(event))
        {
        case KEDR_SWT_COMMON:
            return "Common wait";
        break;
        default:
            return "Unknown wait";
        }
    }
};


/* Thread create before */
template<class T>
class EventPrinterTCBefore: public EventPrinterOneLine<T>
{
public:
    EventPrinterTCBefore(const EventPrinter& eventPrinter):
        EventPrinterOneLine<T>(eventPrinter),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.tcreate_before.pc"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        os << "Begin create thread at " << Addr<T>(pcVar, event);
    }
private:
    const CTFVarInt& pcVar;
};

/* Thread create after */
template<class T>
class EventPrinterTCAfter: public EventPrinterOneLine<T>
{
public:
    EventPrinterTCAfter(const EventPrinter& eventPrinter):
        EventPrinterOneLine<T>(eventPrinter),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.tcreate_after.pc")),
        childTidVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.tcreate_after.child_tid"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        os << "Thread " << Addr<T>(childTidVar, event)
            << " created at " << Addr<T>(pcVar, event);
    }
private:
    const CTFVarInt& pcVar;
    const CTFVarInt& childTidVar;
};

template<class T>
class EventPrinterThreadJoin: public EventPrinterOneLine<T>
{
public:
    EventPrinterThreadJoin(const EventPrinter& eventPrinter):
        EventPrinterOneLine<T>(eventPrinter),
        pcVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.tjoin.pc")),
        childTidVar(findInt(eventPrinter.getTraceReader(),
            "event.fields.tjoin.child_tid"))
        {}
protected:
    void printEventFields(CTFReader::Event& event, std::ostream& os)
    {
        os << "Join to thread " << Addr<T>(childTidVar, event)
            << " at " << Addr<T>(pcVar, event);
    }
private:
    const CTFVarInt& pcVar;
    const CTFVarInt& childTidVar;
};


void EventPrinter::print(CTFReader::Event& event, std::ostream& os) const
{
    int index = eventTypeVar.getValue(event);
    
    typePrinters[index]->print(event, os);
}

template<class T>
void EventPrinter::setupTypePrinters(void)
{
    const CTFTypeEnum* eventTypeEnum = eventTypeVar.getType();
    int nValues = eventTypeEnum->getNValues();
    
    typePrinters.reserve(nValues);
    
    for(int i = 0; i < nValues; i++)
    {
        const std::string& eventType = eventTypeEnum->valueToStr(i);

        //std::cerr << "Possible event type: " << eventType << std::endl;

        EventPrinterType* eventPrinterType;
        if(eventType == "fentry")
            eventPrinterType = new EventPrinterFEntry<T>(*this);
        else if(eventType == "fexit")
            eventPrinterType = new EventPrinterFExit<T>(*this);
        else if(eventType == "fcpre")
            eventPrinterType = new EventPrinterFCPre<T>(*this);
        else if(eventType == "fcpost")
            eventPrinterType = new EventPrinterFCPost<T>(*this);
        else if(eventType == "ma")
            eventPrinterType = new EventPrinterMA<T>(*this);
        else if(eventType == "lma_update")
            eventPrinterType = new EventPrinterLockedUpdate<T>(*this);
        else if(eventType == "lma_read")
            eventPrinterType = new EventPrinterLockedRead<T>(*this);
        else if(eventType == "lma_write")
            eventPrinterType = new EventPrinterLockedWrite<T>(*this);
        else if(eventType == "ioma")
            eventPrinterType = new EventPrinterIO<T>(*this);
        else if(eventType == "mwb")
            eventPrinterType = new EventPrinterMWB<T>(*this);
        else if(eventType == "mrb")
            eventPrinterType = new EventPrinterMRB<T>(*this);
        else if(eventType == "mfb")
            eventPrinterType = new EventPrinterMFB<T>(*this);
        else if(eventType == "alloc")
            eventPrinterType = new EventPrinterAlloc<T>(*this);
        else if(eventType == "free")
            eventPrinterType = new EventPrinterFree<T>(*this);
        else if(eventType == "lock")
            eventPrinterType = new EventPrinterLock<T>(*this);
        else if(eventType == "unlock")
            eventPrinterType = new EventPrinterUnlock<T>(*this);
        else if(eventType == "rlock")
            eventPrinterType = new EventPrinterRLock<T>(*this);
        else if(eventType == "runlock")
            eventPrinterType = new EventPrinterRUnlock<T>(*this);
        else if(eventType == "signal")
            eventPrinterType = new EventPrinterSignal<T>(*this);
        else if(eventType == "wait")
            eventPrinterType = new EventPrinterWait<T>(*this);
        else if(eventType == "tcreate_before")
            eventPrinterType = new EventPrinterTCBefore<T>(*this);
        else if(eventType == "tcreate_after")
            eventPrinterType = new EventPrinterTCAfter<T>(*this);
        else if(eventType == "tjoin")
            eventPrinterType = new EventPrinterThreadJoin<T>(*this);
        else
            eventPrinterType = new EventPrinterUnknown<T>(*this);
        
        typePrinters.push_back(eventPrinterType);
    }
}

EventPrinter::~EventPrinter(void)
{
    for(int i = 0; i < (int)typePrinters.size(); i++)
        delete typePrinters[i];
}


int main(int argc, char** argv)
{
    if(argc < 2)
    {
        std::cerr << "Usage: kedr_simple_converter <directory-with-trace> [stream]";
        return -1;
    }
    
    KEDRTraceReader traceReader(argv[1]);
    
    EventPrinter eventPrinter(traceReader);
    
    if(argc > 2)
    {
        /* Output content of one stream instead of full trace */
        std::string streamFilename = std::string(argv[1]) + '/' + argv[2];
        std::ifstream is(streamFilename.c_str());
        if(!is)
        {
            std::cerr << "Failed to open file '" << streamFilename << "'\n";
            return -1;
        }
        for(CTFReader::EventIterator iter(traceReader, is); iter; ++iter)
        {
            eventPrinter.print(*iter, std::cout);
        }
        return 0;
    }
    
    const CTFVarInt* lost_events_total_var = NULL;
    {
        const CTFVar* var = traceReader.findVar("stream.packet.context.lost_events_total");
        if(var != NULL)
        {
            lost_events_total_var = static_cast<const CTFVarInt*>(var);
            //debug
            std::cerr << "Trace has lost message counter." << std::endl;
        }
    }
    
    for(KEDRTraceReader::EventIterator iter(traceReader); iter; ++iter)
    {
        if(lost_events_total_var && lost_events_total_var->getInt32(*iter))
        {
            std::cerr << "WARNING: There are events lost." << std::endl;
            lost_events_total_var = NULL;
        }
        eventPrinter.print(*iter, std::cout);
    }
    
    return 0;
}