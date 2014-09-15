Go to Search Api settings http://localhost/graph/admin/config/search/search_api
1. Add server with "Neo4j Service"
2. Add Relation index

  a. Add an index on "Relation" item type with the Neo4j Server added before
  b. Go to the fields tab in the index
  c. Index "Relation type", "Endpoints list" + other fields you want to add in Neo4j

3. Add indexes on entities that are involved in relations and you want to add properties (like Node, User)

  Example: Node
  a. Add an index on "Node" item type
  b. Go to fields tab in the index settings
  c. Add what fields you want to index, like tags, description ...
