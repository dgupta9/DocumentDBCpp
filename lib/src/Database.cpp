/***
* The MIT License (MIT)
*
* Copyright (c) 2015 DocumentDBCpp
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
***/

#include "Database.h"

#include <cpprest/filestream.h>
#include <cpprest/json.h>

#include "ConnectionHelper.h"
#include "DocumentDBConstants.h"
#include "IndexingPolicy.h"
#include "exceptions.h"

using namespace documentdb;
using namespace std;
using namespace utility;
using namespace web::http;
using namespace web::json;
using namespace web::http::client;

Database::Database(
		const shared_ptr<const DocumentDBConfiguration>& document_db_configuration,
		const string_t& id,
		const string_t& resource_id,
		const unsigned long ts,
		const string_t& self,
		const string_t& etag,
		const string_t& colls,
		const string_t& users)
	: DocumentDBEntity(document_db_configuration, id, resource_id, ts, self, etag)
	, colls_(colls)
	, users_(users)
{
	// TODO: deal with users, triggers, sprocs...
}


Database::~Database()
{
}

shared_ptr<Collection> Database::CollectionFromJson(
	const value* json_collection) const
{
	string_t id = json_collection->at(DOCUMENT_ID).as_string();
	string_t rid = json_collection->at(RESPONSE_RESOURCE_RID).as_string();
	unsigned long ts = json_collection->at(RESPONSE_RESOURCE_TS).as_integer();
	string_t self = this->self() + _XPLATSTR("/") + colls_ + id ;
	string_t etag = json_collection->at(RESPONSE_RESOURCE_ETAG).as_string();
	string_t docs = json_collection->at(RESPONSE_RESOURCE_DOCS).as_string();
	string_t sprocs = json_collection->at(RESPONSE_RESOURCE_SPROCS).as_string();
	string_t triggers = json_collection->at(RESPONSE_RESOURCE_TRIGGERS).as_string();
	string_t udfs = json_collection->at(RESPONSE_RESOURCE_UDFS).as_string();
	string_t conflicts = json_collection->at(RESPONSE_RESOURCE_CONFLICTS).as_string();

	IndexingPolicy indexing_policy;
	if (json_collection->has_field(RESPONSE_INDEXING_POLICY))
	{
		value indexing_policy_json = json_collection->at(RESPONSE_INDEXING_POLICY);
		indexing_policy = IndexingPolicy::FromJson(indexing_policy_json);
	}

	return make_shared<Collection>(
		this->document_db_configuration(),
		id,
		rid,
		ts,
		self,
		etag,
		docs,
		sprocs,
		triggers,
		udfs,
		conflicts,
		indexing_policy);
}

shared_ptr<User> Database::UserFromJson(
	const value* json_user) const
{
	string_t id = json_user->at(DOCUMENT_ID).as_string();
	string_t rid = json_user->at(RESPONSE_RESOURCE_RID).as_string();
	unsigned long ts = json_user->at(RESPONSE_RESOURCE_TS).as_integer();
	string_t self = json_user->at(RESPONSE_RESOURCE_SELF).as_string();
	string_t etag = json_user->at(RESPONSE_RESOURCE_ETAG).as_string();
	string_t permissions = json_user->at(RESPONSE_RESOURCE_PERMISSIONS).as_string();

	IndexingPolicy indexing_policy;
	if (json_user->has_field(RESPONSE_INDEXING_POLICY))
	{
		value indexing_policy_json = json_user->at(RESPONSE_INDEXING_POLICY);
		indexing_policy = IndexingPolicy::FromJson(indexing_policy_json);
	}

	return make_shared<User>(
		this->document_db_configuration(),
		id,
		rid,
		ts,
		self,
		etag,
		permissions);
}

pplx::task<shared_ptr<Collection>> Database::CreateCollectionAsync(
	const string_t& id, const utility::string_t& partition_key) const
{
	http_request request = CreateRequest(
		methods::POST,
		RESOURCE_PATH_COLLS,
		this->self(),
		this->document_db_configuration()->master_key());
	request.set_request_uri(this->self() + _XPLATSTR("/") + RESOURCE_PATH_COLLS);

	value body, partitionKey;
	vector<value> partitionPaths;
	partitionPaths.push_back(value::string(_XPLATSTR("/")+partition_key));
	partitionKey[PATHS] = value::array(partitionPaths);
	partitionKey[RESPONSE_INDEX_KIND] = value::string(L"Hash");
	partitionKey[PARTITION_VERSION] = value::number(2);
	body[PARTITION_KEY] = partitionKey;
	body[DOCUMENT_ID] = value::string(id);
	request.set_body(body);

	return this->document_db_configuration()->http_client().request(request).then([=](http_response response)
	{
		value json_response = response.extract_json().get();

		if (response.status_code() == status_codes::Created)
		{
			return CollectionFromJson(&json_response);
		}

		ThrowExceptionFromResponse(response.status_code(), json_response);
	});
}

shared_ptr<Collection> Database::CreateCollection(
	const string_t& id, const utility::string_t& partition_key) const
{
	return this->CreateCollectionAsync(id, partition_key).get();
}

pplx::task<void> Database::DeleteCollectionAsync(
	const shared_ptr<Collection>& collection) const
{
	return DeleteCollectionAsync(collection->resource_id());
}

void Database::DeleteCollection(
	const shared_ptr<Collection>& collection) const
{
	this->DeleteCollectionAsync(collection).get();
}

pplx::task<void> Database::DeleteCollectionAsync(
	const string_t& resource_id) const
{
	http_request request = CreateRequest(
		methods::DEL,
		RESOURCE_PATH_COLLS,
		resource_id,
		this->document_db_configuration()->master_key());
	request.set_request_uri(this->self() + colls_ + resource_id);

	return this->document_db_configuration()->http_client().request(request).then([=](http_response response)
	{
		if (response.status_code() == status_codes::NoContent)
		{
			return;
		}

		value json_response = response.extract_json().get();
		ThrowExceptionFromResponse(response.status_code(), json_response);
	});
}

void Database::DeleteCollection(
	const string_t& resource_id) const
{
	this->DeleteCollectionAsync(resource_id).get();
}

pplx::task<shared_ptr<Collection>> Database::GetCollectionAsync(
	const string_t& resource_id) const
{
	http_request request = CreateRequest(
		methods::GET,
		RESOURCE_PATH_COLLS,
		this->self() + _XPLATSTR("/") + colls_ + resource_id,
		this->document_db_configuration()->master_key());
	request.set_request_uri(this->self() + _XPLATSTR("/") + colls_ + resource_id);

	return this->document_db_configuration()->http_client().request(request).then([=](http_response response)
	{
		value json_response = response.extract_json().get();

		if (response.status_code() == status_codes::OK)
		{
			return CollectionFromJson(&json_response);
		}

		ThrowExceptionFromResponse(response.status_code(), json_response);
	});
}

shared_ptr<Collection> Database::GetCollection(
	const string_t& resource_id) const
{
	return this->GetCollectionAsync(resource_id).get();
}

pplx::task<vector<shared_ptr<Collection>>> Database::ListCollectionsAsync() const
{
	http_request request = CreateRequest(
		methods::GET,
		RESOURCE_PATH_COLLS,
		this->self(),
		this->document_db_configuration()->master_key());
	request.set_request_uri(this->self() + _XPLATSTR("/") + RESOURCE_PATH_COLLS);
	return this->document_db_configuration()->http_client().request(request).then([=](http_response response)
	{
		value json_response = response.extract_json().get();

		if (response.status_code() == status_codes::OK)
		{
			assert(this->resource_id() == json_response.at(RESPONSE_RESOURCE_RID).as_string());
			vector<shared_ptr<Collection>> collections;
			collections.reserve(json_response.at(RESPONSE_BODY_COUNT).as_integer());
			value json_collections = json_response.at(RESPONSE_DOCUMENT_COLLECTIONS);

			for (auto iter = json_collections.as_array().cbegin(); iter != json_collections.as_array().cend(); ++iter)
			{
				shared_ptr<Collection> coll = CollectionFromJson(&(*iter));
				collections.push_back(coll);
			}
			return collections;
		}

		ThrowExceptionFromResponse(response.status_code(), json_response);
	});
}

vector<shared_ptr<Collection>> Database::ListCollections() const
{
	return this->ListCollectionsAsync().get();
}

pplx::task<shared_ptr<User>> Database::CreateUserAsync(
	const string_t& id) const
{
	http_request request = CreateRequest(
		methods::POST,
		RESOURCE_PATH_USERS,
		this->resource_id(),
		this->document_db_configuration()->master_key());
	request.set_request_uri(this->self() + users_);

	value body;
	body[DOCUMENT_ID] = value::string(id);
	request.set_body(body);

	return this->document_db_configuration()->http_client().request(request).then([=](http_response response)
	{
		value json_response = response.extract_json().get();

		if (response.status_code() == status_codes::Created)
		{
			return UserFromJson(&json_response);
		}

		ThrowExceptionFromResponse(response.status_code(), json_response);
	});

}

shared_ptr<User> Database::CreateUser(
	const string_t& id) const
{
	return this->CreateUserAsync(id).get();
}

pplx::task<void> Database::DeleteUserAsync(
	const string_t& resource_id) const
{
	http_request request = CreateRequest(
		methods::DEL,
		RESOURCE_PATH_USERS,
		resource_id,
		this->document_db_configuration()->master_key());
	request.set_request_uri(this->self() + users_ + resource_id);

	return this->document_db_configuration()->http_client().request(request).then([=](http_response response)
	{
		if (response.status_code() == status_codes::NoContent)
		{
			return;
		}
		value json_response = response.extract_json().get();
		ThrowExceptionFromResponse(response.status_code(), json_response);
	});
}

void Database::DeleteUser(
	const string_t& resource_id) const
{
	this->DeleteUserAsync(resource_id).get();
}

pplx::task<void> Database::DeleteUserAsync(
	const shared_ptr<User>& user) const
{
	return this->DeleteUserAsync(user->resource_id());
}

void Database::DeleteUser(
	const shared_ptr<User>& user) const
{
	this->DeleteUserAsync(user->resource_id()).get();
}

pplx::task<shared_ptr<User>> Database::GetUserAsync(
	const string_t& resource_id) const
{
	http_request request = CreateRequest(
		methods::GET,
		RESOURCE_PATH_USERS,
		resource_id,
		this->document_db_configuration()->master_key());
	request.set_request_uri(this->self() + users_ + resource_id);

	return this->document_db_configuration()->http_client().request(request).then([=](http_response response)
	{
		value json_response = response.extract_json().get();

		if (response.status_code() == status_codes::OK)
		{
			return UserFromJson(&json_response);
		}

		ThrowExceptionFromResponse(response.status_code(), json_response);
	});
}

shared_ptr<User> Database::GetUser(
	const string_t& resource_id) const
{
	return this->GetUserAsync(resource_id).get();
}

pplx::task<vector<shared_ptr<User>>> Database::ListUsersAsync() const
{
	http_request request = CreateRequest(
		methods::GET,
		RESOURCE_PATH_USERS,
		this->resource_id(),
		this->document_db_configuration()->master_key());
	request.set_request_uri(this->self() + users_);
	return this->document_db_configuration()->http_client().request(request).then([=](http_response response)
	{
		value json_response = response.extract_json().get();

		if (response.status_code() == status_codes::OK)
		{
			assert(this->resource_id() == json_response.at(RESPONSE_RESOURCE_RID).as_string());
			vector<shared_ptr<User>> users;
			users.reserve(json_response.at(RESPONSE_BODY_COUNT).as_integer());
			value json_users = json_response.at(RESPONSE_USERS);

			for (auto iter = json_users.as_array().cbegin(); iter != json_users.as_array().cend(); ++iter)
			{
				shared_ptr<User> user = UserFromJson(&(*iter));
				users.push_back(user);
			}
			return users;
		}

		ThrowExceptionFromResponse(response.status_code(), json_response);
	});
}

pplx::task<shared_ptr<User>> Database::ReplaceUserAsync(
	const string_t& resource_id,
	const string_t& new_id) const
{
	http_request request = CreateRequest(
		methods::PUT,
		RESOURCE_PATH_USERS,
		resource_id,
		document_db_configuration()->master_key());
	request.set_request_uri(this->self() + users_ + resource_id);

	value body;
	body[DOCUMENT_ID] = value::string(new_id);
	request.set_body(body);

	return document_db_configuration()->http_client().request(request).then([=](http_response response)
	{
		value json_response = response.extract_json().get();

		if (response.status_code() == status_codes::OK)
		{
			return UserFromJson(&json_response);
		}

		ThrowExceptionFromResponse(response.status_code(), json_response);
	});
}

shared_ptr<User> Database::ReplaceUser(
	const string_t& resource_id,
	const string_t& new_id) const
{
	return this->ReplaceUserAsync(resource_id, new_id).get();
}

vector<shared_ptr<User>> Database::ListUsers() const
{
	return ListUsersAsync().get();
}
