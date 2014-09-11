<?php

class SearchApiNeo4jService extends SearchApiAbstractService {
  protected $connection;
  
  function __construct(SearchApiServer $server) {
    parent::__construct($server);
    $this->server = $server;
    $this->options = &$server->options;
    $this->connection = NeoFactory::getClient($server);
  }
  
  function configurationForm(array $form, array &$form_state) {
    if($this->options) {
      $form['server_description'] = array(
        '#type' => 'item',
        '#title' => 'Neo4j server URI',
        '#description' => 'test',
      );
    }
    
    $form['host'] = array(
      '#type' => 'textfield',
      '#title' => 'Host',
      '#default_value' => 'localhost',
      '#required' => TRUE,
    );
    $form['port'] = array(
      '#type' => 'textfield',
      '#title' => 'Port',
      '#default_value' => '7474',
      '#required' => TRUE,
    );
    
    return $form;
  }

  function configurationFormSubmit(array $form, array &$values, array &$form_state) {
    parent::configurationFormSubmit($form, $values, $form_state);
  }
  
  function addIndex(\SearchApiIndex $index) {
    parent::addIndex($index);
  }
  
  function removeIndex($index) {
    parent::removeIndex($index);
  }
  
  function create_id($item_type, $id) {
    return $item_type . '_' . $id;
  }
  
  function indexNode() {
    $node = $node_index->findOne('uuid', $uuid);

    if(!$node) {
      $node = $this->connection->createNode();
    }
    
    $node->setProperty('uuid', $uuid);        

    foreach($item as $field_name => $field) {
      if(!empty($field['value'])) {
        $node->setProperty($field_name, $field['value']);
      }
    }
    
    $node->save();
  }
  
  function indexItem($index, $item) {
    $index_type = ($index->item_type == 'relation') ? 'relation' : 'node';
    
    $global_index = $this->connection->getIndex($index_type . '_index', $index_type);
    $current_index = $this->connection->getIndex($index_name, $index_type);
        
    $element = $global_index->findOne('uuid', $uuid);
    
    $global_index->remove($element, 'uuid');
    $global_index->add($element, 'uuid', $uuid);
        
    $current_index->remove($element);
    $properties = $element->getProperties();
    foreach($properties as $name => $value) {
      $current_index->add($element, $name, $value);
    }
  }
  
  function indexItems(SearchApiIndex $index, array $items) {
    //dpm($items);
    $index_type = ($index->item_type == 'relation') ? 'relation' : 'node';
    $index_name = $index->name;
    
    $current_index = $this->connection->getIndex($index_name, $index_type);
    $relation_index = $this->connection->getIndex('relation_index', 'relation');
    $node_index = $this->connection->getIndex('node_index');
    
    foreach($items as $id => $item) {
      if($index->item_type == 'relation') {     
        $endpoint_fields = array(
          'source' => 'endpoints_source_node', 
          'target' => 'endpoints_target_node'
        );
        
        if(!empty($item['endpoints_source_node'])) {
          $source = $item['endpoints_source_node'];
          
          $id = reset($source['value']);
          $uuid = $this->create_id($source['entity_type'], $id);
          $node = $node_index->findOne('uuid', $uuid);
          
          if(!$node) {
            $node = $this->connection->createNode();
          }
//        $node = $this->connection->getNodeByProperty('uuid', $uuid);
          $node->setProperty('uuid', $uuid);
          $source_node = $node->save();
          
          //Remove from index and reindex
          $node_index->remove($node, 'uuid');
          $node_index->add($node, 'uuid', $uuid);
        }
        
        if(!empty($item['endpoints_target_node'])) {
          $source = $item['endpoints_target_node'];
          
          $id = reset($source['value']);
          $uuid = $this->create_id($source['entity_type'], $id);
          $node = $node_index->findOne('uuid', $uuid);
          
          if(!$node) {
            $node = $this->connection->createNode();
          }
          
          $node->setProperty('uuid', $uuid);
          $target_node = $node->save();
          
          //Remove from index and reindex
          $node_index->remove($node, 'uuid');
          $node_index->add($node, 'uuid', $uuid);
          
//          $node = $this->connection->getNodeByProperty('uuid', $uuid);
//          $node->setProperty('uuid', $uuid);
//          $target_node = $node->save();
        }
        
        //Create relation
        $relation_type = $item['relation_type']['value'];
        $uuid = $this->create_id($index->item_type, $id);

        //Check if relation exists
        $relation = $relation_index->findOne('uuid', $uuid);
        
        if(!$relation) {
          $relation = $source_node->relateTo($target_node, $relation_type);
          $relation->setProperty('uuid', $uuid);
        }
        
        foreach($item as $field_name => $field) {
          //Skip endpoint fields
          if(in_array($field_name, $endpoint_fields)) {
            continue;          
          }
          $relation->setProperty($field_name, $field['value']);
          //@TODO - CAST VALUES
        }
        $relation->save();
        
        //Add to global relation index
        $relation_index->remove($relation, 'uuid');
        $relation_index->add($relation, 'uuid', $uuid);
        
        //Add to currentindex
        $current_index->remove($relation);
        $properties = $relation->getProperties();
        foreach($properties as $name => $value) {
          $current_index->add($relation, $name, $value);
        }
      }
      else {
        $uuid = $this->create_id($index->item_type, $id);
        
        //get uuid from node index
        /*
         * if not exists create
         *  1) index in node index
         *  2) index in $index
         */
        /* else if exists
         *  1) delete from node index + reindex
         *  2) delete from $index + reindex
         */
        $node = $node_index->findOne('uuid', $uuid);

        if(!$node) {
          $node = $this->connection->createNode();
        }
//        $engine = $this->connection->queryEngine('Cypher');
//        
//        $result = $engine->query(
//            'MATCH (n { uuid: "' . $uuid . '" }) RETURN n LIMIT 1')
//            ->getResultSet();
//        
//        $exists = $result->count();
////        
//        if(!$exists) {
//          $node = $this->connection->createNode();
//        }
//        else {
//          $row = $result->current();
//          $id = $row['n']->getId();
//          $node = $this->connection->getNode($id); 
//        }
        
        //update node
        $node->setProperty('uuid', $uuid);        
        
        foreach($item as $field_name => $field) {
          if(!empty($field['value'])) {
            //@TODO - cast values
            $node->setProperty($field_name, $field['value']);
          }
        }
        
        $node->save();
        
        $node_index->remove($node, 'uuid');
        $node_index->add($node, 'uuid', $uuid);
        
        $current_index->remove($node);
        $properties = $node->getProperties();
        foreach($properties as $name => $value) {
          $current_index->add($node, $name, $value);
        }
          
      }
    }
    return array();
  }
  
  function deleteItems($ids = 'all', \SearchApiIndex $index = NULL) {
    $db = debug_backtrace();
    dpm($db);
    //Delete from global index and normal index
  }
  
  function search(\SearchApiQueryInterface $query) {
    ;
  }
}