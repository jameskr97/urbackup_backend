#pragma once

#include "../Interface/Database.h"
#include "../Interface/Types.h"
#include "../Interface/Mutex.h"
#include "../Interface/Condition.h"
#include "../Interface/Thread.h"
#include <memory.h>

const size_t bytes_in_index = 16;

class FileIndex : public IThread
{
public:
	typedef db_results(*get_data_callback_t)(size_t, void *userdata);

#pragma pack(1)
	struct SIndexKey
	{
		SIndexKey(const char thash[bytes_in_index], int64 filesize, int clientid)
			: filesize(filesize), clientid(clientid)
		{
			memcpy(hash, thash, bytes_in_index);
		}

		SIndexKey(const char thash[bytes_in_index], int64 filesize)
			: filesize(filesize), clientid(0)
		{
			memcpy(hash, thash, bytes_in_index);
		}

		SIndexKey(void)
			: filesize(0), clientid(0)
		{
			memset(hash, 0, bytes_in_index);
		}

		void operator=(const SIndexKey& other)
		{
			memcpy(hash, other.hash, bytes_in_index);
			filesize=other.filesize;
			clientid=other.clientid;
		}

		bool operator==(const SIndexKey& other) const
		{
			return memcmp(hash, other.hash, bytes_in_index)==0 && filesize==other.filesize
				&& clientid==other.clientid;
		}

		bool operator!=(const SIndexKey& other) const
		{
			return !(*this==other);
		}

		bool operator<(const SIndexKey& other) const
		{
			int mres=memcmp(hash, other.hash, bytes_in_index);
			return mres<0 ||
				(mres==0 && filesize<other.filesize) ||
				(mres==0 && filesize==other.filesize && clientid<other.clientid);
		}

		bool isEqualWithoutClientid(const SIndexKey& other)
		{
			return memcmp(hash, other.hash, bytes_in_index)==0 && filesize==other.filesize;
		}

		char hash[bytes_in_index];
		int64 filesize;
		int clientid;
	};
#pragma pack()

	virtual ~FileIndex(void) {};

	virtual bool has_error(void)=0;

	virtual void create(get_data_callback_t get_data_callback, void *userdata)=0;

	virtual int64 get(const SIndexKey& key)=0;

	virtual int64 get_any_client(const SIndexKey& key) = 0;

	virtual int64 get_prefer_client(const SIndexKey& key) = 0;

	virtual std::map<int, int64> get_all_clients(const SIndexKey& key) = 0;

	virtual void start_transaction(void)=0;

	virtual void put(const SIndexKey& key, int64 value)=0;

	static void put_delayed(const SIndexKey& key, int64 value);

	virtual int64 get_with_cache(const SIndexKey& key);

	virtual int64 get_with_cache_exact(const SIndexKey& key);

	virtual std::map<int, int64> get_all_clients_with_cache(const SIndexKey& key);

	virtual int64 get_with_cache_prefer_client(const SIndexKey& key);

	virtual void del(const SIndexKey& key)=0;

	static void del_delayed(const SIndexKey& key);

	virtual void commit_transaction(void)=0;

	virtual void start_iteration()=0;

	virtual std::map<int, int64> get_next_entries_iteration(bool& has_next)=0;

	virtual void stop_iteration()=0;

	void operator()(void);

	static void shutdown();

private:

	bool get_from_cache( const FileIndex::SIndexKey &key, const std::map<SIndexKey, int64>& cache, int64& res );

	bool get_from_cache_prefer_client( const SIndexKey &key, const std::map<SIndexKey, int64>& cache, int64& res);

	bool get_from_cache_exact( const SIndexKey& key, const std::map<SIndexKey, int64>& cache, int64& res );

	void get_from_cache_all_clients( const SIndexKey &key, const std::map<SIndexKey, int64>& cache, std::map<int, int64> &ret );

	static std::map<SIndexKey, int64> cache_buffer_1;
	static std::map<SIndexKey, int64> cache_buffer_2;
	static std::map<SIndexKey, int64>* active_cache_buffer;
	static std::map<SIndexKey, int64>* other_cache_buffer;
	static std::map<SIndexKey, bool> del_buffer;
	static IMutex *mutex;
	static ICondition *cond;
	static bool do_shutdown;
};