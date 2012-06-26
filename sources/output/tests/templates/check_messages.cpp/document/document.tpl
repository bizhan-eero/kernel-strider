/* Checks that all messages in the trace are expected ones */

#include <kedr/kedr_trace_reader/kedr_trace_reader.h>
#include <kedr/object_types.h>
#include <stdexcept>

#include <iostream>

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


/* Auxiliary functions */
static int check_int_variable(CTFContext& context,
	const CTFReader& reader, const std::string& varName,
	uint64_t valExpected, bool isHex)
{
	const CTFVarInt& varInt = findInt(reader, varName);
	
	uint64_t val = varInt.getUInt64(context);
	if(val != valExpected)
	{
		std::ios_base::fmtflags oldFlags = std::cerr.flags();
		
		if(isHex) std::cerr << std::hex << std::showbase;
		
		std::cerr << "Expected, that value of variable '" << varName
			<< "' will be " << valExpected << ", but it is " << val << ".\n";
		
		std::cerr.flags(oldFlags);
		
		return -1;
	}
	
	return 0;
}

static int check_tid(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tidExpected)
{
	return check_int_variable(event, traceReader,
		"stream.event.context.tid", tidExpected, true);
}

static int check_type(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, const std::string& typeExpected)
{
	const CTFVarEnum& varEnum = findEnum(traceReader, "stream.event.header.type");
	
	const std::string& type = varEnum.getEnum(event);
	
	if(type == "")
	{
		std::cerr << "Invalid event type.\n";
		return -1;
	}
	
	if(type != typeExpected)
	{
		std::cerr << "Expected that type of event will be '"
			<< typeExpected << "', but it is '" << type << "'.\n";
		return -1;
	}
	
	return 0;
}


/* Checkers */
static inline int check_message_function_entry(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t func)
{
	if(check_type(event, traceReader, "fentry")
		|| check_tid(event, traceReader, tid)
		|| check_int_variable(event, traceReader,
			"event.fields.fentry.func", func, true))
	{
		std::cerr << "Function entry event is incorrect.\n";
		return -1;
	}
	return 0;
}

static inline int check_message_function_exit(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t func)
{
	if(check_type(event, traceReader, "fexit")
		|| check_tid(event, traceReader, tid)
		|| check_int_variable(event, traceReader,
			"event.fields.fexit.func", func, true))
	{
		std::cerr << "Function exit event is incorrect.\n";
		return -1;
	}
	return 0;
}

static inline int check_message_call_pre(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t func)
{
	if(check_type(event, traceReader, "fcpre")
		|| check_tid(event, traceReader, tid)
		|| check_int_variable(event, traceReader,
			"event.fields.fcpre.pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields.fcpre.func", func, true))
	{
		std::cerr << "Function call pre event is incorrect.\n";
		return -1;
	}
	return 0;
}

static inline int check_message_call_post(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t func)
{
	if(check_type(event, traceReader, "fcpost")
		|| check_tid(event, traceReader, tid)
		|| check_int_variable(event, traceReader,
			"event.fields.fcpost.pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields.fcpost.func", func, true))
	{
		std::cerr << "Function call post event is incorrect.\n";
		return -1;
	}
	return 0;
}


class check_message_memory_events
{
public:
	check_message_memory_events(CTFReader::Event& event,
		const KEDRTraceReader& traceReader)
		: event(event), traceReader(traceReader) {}
			
	int check_begin(uint64_t tid, int n_subevents)
	{
		if(check_type(event, traceReader, "ma")
			|| check_tid(event, traceReader, tid)
			|| check_int_variable(event, traceReader,
				"event.context.ma.n_subevents", n_subevents, false))
		{
			std::cerr << "Memory access event event is incorrect.\n";
			return -1;
		}
		
		elemIterator = CTFVarArray::ElemIterator(
			findArray(traceReader, "event.fields.ma"), event);
		index = 0;
		
		return 0;
	}
	int check_submessage(uint64_t pc, uint64_t addr, uint64_t size, int access_type)
	{
		if(check_int_variable(*elemIterator, traceReader,
			"event.fields.ma[].pc", pc, true)
			|| check_int_variable(*elemIterator, traceReader,
			"event.fields.ma[].addr", addr, true)
			|| check_int_variable(*elemIterator, traceReader,
			"event.fields.ma[].size", size, false)
			|| check_int_variable(*elemIterator, traceReader,
			"event.fields.ma[].access_type", access_type, true))
		{
			std::cerr << index + 1 << "-th subevent in memory accesses "
				"event is incorrect.\n";
			return -1;
		}
		++elemIterator; ++index;
		
		return 0;
	}

private:
	CTFVarArray::ElemIterator elemIterator;
	int index;
	
	CTFReader::Event& event;
	const KEDRTraceReader& traceReader;
};

static inline int check_message_locked_op(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t addr, uint64_t size, enum kedr_memory_event_type type)
{
	std::string type_str;
	switch(type)
	{
	case KEDR_ET_MREAD:
		type_str = "lma_read";
	break;
	case KEDR_ET_MWRITE:
		type_str = "lma_write";
	break;
	case KEDR_ET_MUPDATE:
		type_str = "lma_update";
	break;
	default:
		throw std::logic_error("Incorrect memory access type");
	}
	
	if(check_tid(event, traceReader, tid)
		|| check_type(event, traceReader, type_str)
		|| check_int_variable(event, traceReader,
			"event.fields." + type_str + ".pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields." + type_str + ".addr", addr, true)
		|| check_int_variable(event, traceReader,
			"event.fields." + type_str + ".size", size, false))
	{
		std::cerr << "Locked operation event is incorrect.\n";
		return -1;
	}

	return 0;
}

static inline int check_message_io_mem_op(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t addr, uint64_t size, enum kedr_memory_event_type type)
{
	if(check_tid(event, traceReader, tid)
		||check_type(event, traceReader, "ioma")
		|| check_int_variable(event, traceReader,
			"event.fields.ioma.pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields.ioma.addr", addr, true)
		|| check_int_variable(event, traceReader,
			"event.fields.ioma.size", size, false)
		|| check_int_variable(event, traceReader,
			"event.fields.ioma.access_type", type, false))
	{
		std::cerr << "I/O operation event is incorrect.\n";
		return -1;
	}
	
	return 0;
}

static inline int check_message_memory_barrier(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	enum kedr_barrier_type type)
{
	std::string type_str;
	switch(type)
	{
	case KEDR_BT_FULL:
		type_str = "mfb";
	break;
	case KEDR_BT_LOAD:
		type_str = "mrb";
	break;
	case KEDR_BT_STORE:
		type_str = "mwb";
	break;
	default:
		throw std::logic_error("Incorrect memory barrier type");
	}
	
	if(check_tid(event, traceReader, tid)
		|| check_type(event, traceReader, type_str)
		|| check_int_variable(event, traceReader,
			"event.fields." + type_str + ".pc", pc, true))
	{
		std::cerr << "Memory barrier event is incorrect.\n";
		return -1;
	}

	return 0;
}

static inline int check_message_alloc(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t size, uint64_t addr)
{
	if(check_tid(event, traceReader, tid)
		||check_type(event, traceReader, "alloc")
		|| check_int_variable(event, traceReader,
			"event.fields.alloc.pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields.alloc.size", size, false)
		|| check_int_variable(event, traceReader,
			"event.fields.alloc.pointer", addr, true))
	{
		std::cerr << "Alloc event is incorrect.\n";
		return -1;
	}
	
	return 0;
}

static inline int check_message_free(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t addr)
{
	if(check_tid(event, traceReader, tid)
		||check_type(event, traceReader, "free")
		|| check_int_variable(event, traceReader,
			"event.fields.free.pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields.free.pointer", addr, true))
	{
		std::cerr << "Free event is incorrect.\n";
		return -1;
	}
	
	return 0;
}

static inline int check_message_lock(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t lock_id, enum kedr_lock_type type)
{
	std::string type_str;
	switch(type)
	{
	case KEDR_LT_RLOCK:
		type_str = "rlock";
	break;
	default:
		type_str = "lock";
	}
	
	if(check_tid(event, traceReader, tid)
		|| check_type(event, traceReader, type_str)
		|| check_int_variable(event, traceReader,
			"event.fields." + type_str + ".pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields." + type_str + ".object", lock_id, true)
		|| check_int_variable(event, traceReader,
			"event.fields." + type_str + ".type", type, false))
	{
		std::cerr << "Lock event is incorrect.\n";
		return -1;
	}

	return 0;
}

static inline int check_message_unlock(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t lock_id, enum kedr_lock_type type)
{
	std::string type_str;
	switch(type)
	{
	case KEDR_LT_RLOCK:
		type_str = "runlock";
	break;
	default:
		type_str = "unlock";
	}
	
	if(check_tid(event, traceReader, tid)
		|| check_type(event, traceReader, type_str)
		|| check_int_variable(event, traceReader,
			"event.fields." + type_str + ".pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields." + type_str + ".object", lock_id, true)
		|| check_int_variable(event, traceReader,
			"event.fields." + type_str + ".type", type, false))
	{
		std::cerr << "Unlock event is incorrect.\n";
		return -1;
	}
	return 0;

}

static inline int check_message_signal(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t obj_id, enum kedr_sw_object_type type)
{
	if(check_tid(event, traceReader, tid)
		|| check_type(event, traceReader, "signal")
		|| check_int_variable(event, traceReader,
			"event.fields.signal.pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields.signal.object", obj_id, true)
		|| check_int_variable(event, traceReader,
			"event.fields.signal.type", type, false))
	{
		std::cerr << "Signal event is incorrect.\n";
		return -1;
	}

	return 0;
}

static inline int check_message_wait(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t obj_id, enum kedr_sw_object_type type)
{
	if(check_tid(event, traceReader, tid)
		|| check_type(event, traceReader, "wait")
		|| check_int_variable(event, traceReader,
			"event.fields.wait.pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields.wait.object", obj_id, true)
		|| check_int_variable(event, traceReader,
			"event.fields.wait.type", type, false))
	{
		std::cerr << "Wait event is incorrect.\n";
		return -1;
	}

	return 0;
}

static inline int check_message_thread_create(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t child_tid)
{
	if(check_tid(event, traceReader, tid)
		|| check_type(event, traceReader, "tcreate")
		|| check_int_variable(event, traceReader,
			"event.fields.tcreate.pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields.tcreate.child_tid", child_tid, true))
	{
		std::cerr << "Thread create event is incorrect.\n";
		return -1;
	}
	return 0;

}

static inline int check_message_thread_join(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t pc,
	uint64_t child_tid)
{
	if(check_tid(event, traceReader, tid)
		|| check_type(event, traceReader, "tjoin")
		|| check_int_variable(event, traceReader,
			"event.fields.tjoin.pc", pc, true)
		|| check_int_variable(event, traceReader,
			"event.fields.tjoin.child_tid", child_tid, true))
	{
		std::cerr << "Thread join event is incorrect.\n";
		return -1;
	}

	return 0;
}


int main(int argc, char** argv)
{
    if(argc < 2)
    {
        std::cerr << "Argument with trace directory name should be supplied.\n";
        return -1;
    }
    
    KEDRTraceReader traceReader(argv[1]);
    
    int i = 0;
    KEDRTraceReader::EventIterator iter(traceReader);

	<$constant_def: join(\n\t)$>

	int result;
	
	<$block: join(\n\t)$>
    
    if(iter)
    {
		std::cerr << "Expected, that trace will contain " << i
			<< " events, but it contains more.\n";
		return -1;
    }
    
    return 0;
}