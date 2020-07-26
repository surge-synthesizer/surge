-- Initialize the SQLite DB for the Patch Browser

CREATE TABLE surge_metadata (
    schema_version integer
)

CREATE TABLE patches (
    id integer primary key,
    filename varchar(1024),
    patchname varchar( 256 ),
    author varchar(256),

    comment varchar(4096),

    factoryThirdPartyOrUser integer
);

CREATE TABLE patchtag (
    id integer primary key,
    patchid integer,
    tag integer
);

CREATE TABLE tags (
   id integer primary key,
   tagcagetory integer,
   tag varchar(256) 
);

CREATE TABLE tagcategory (
   id integer primary key,
   tagcategory varchar( 256 )
);
