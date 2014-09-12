<?php

class SearchApiNeo4jService extends SearchApiAbstractService {
  protected $connection;
  private $node_index;
  private $relation_index;
  
  function __construct(SearchApiServer $server) {
    parent::__construct($server);
    $this->server = $server;
    $this->options = &$server->options;
    $this->connection = NeoFactory::getClient($server);
    
    //Init default indexes
    $this->node_index = $this->connection->getIndex('node_index', 'node');
    $this->relation_index = $this->connection->getIndex('relation_index', 'relation');
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
  
  static function cast($value, $type, $original_type) {
    if(is_array($value)) {
      $type = substr($type, 5, -1);
    }
    
    //@TODO - make use of original type
    switch ($type) {
      case 'duration':
      case 'integer':
      case 'date':
        return is_array($value) ? array_map('intval', $value) : intval($value);
        break;
      
      default :
        return $value;
    }
  }
  
  function getDefaultIndex($element_type) {
    if($element_type == 'relation') {
      return $this->relation_index;
    }
    return $this->node_index;
  }

  function getElement($element_type, $uuid) {
    $element_index = $this->getDefaultIndex($element_type);
    return $element_index->findOne('uuid', $uuid);
  }

  function createEndpoint($endpoint){
    $entity_id = reset($endpoint['value']);
    $entity_type = $endpoint['entity_type'];

    $uuid = $this->create_id($entity_type, $entity_id);
    $node = $this->getElement('node', $uuid);
    if(!$node) {
      $node = $this->connection->createElement('node');
    }
    $this->updateElement($node, 'node', $uuid);
    $this->indexElement($node, $this->node_index, array('uuid'));

    return $node;
  }

  function updateElement($element, $element_type, $uuid, $item = array()) {
    if($element_type == 'relation') {
      //Create relation
      $relation_type = $item['relation_type']['value'];
      
      $source_node = $this->createEndpoint($item['endpoints_source_node']);
      $target_node = $this->createEndpoint($item['endpoints_target_node']);

      //Update Relation
      $element->setStartNode($source_node)
        ->setEndNode($target_node)
        ->setType($relation_type);
    }

    $element->setProperty('uuid', $uuid);
    $fields = $this->getFields($element_type, $item);
    foreach($fields as $field_name) {
      $field = $item[$field_name];
      
      if(!empty($field['value'])) {
        //@TODO - cast values
        $value = $this::cast($field['value'], $field['type'], $field['original_type']);
        $element->setProperty($field_name, $value);
      }
    }
    
    $element->save();
  }

  function indexElement($element, $index, $fields = array()) {
    $index->remove($element);

    foreach($fields as $field_name) {
      $index->add($element, $field_name, $element->getProperty($field_name));
    }
  }

  function getFields($element_type, $item) {
    $skip_fields = array(
        'relation' => array(
          'endpoints_source_node', 
          'endpoints_target_node',
          'relation_type',
        ),
        'node' => array(), 
      );
    $fields = array_diff(array_keys($item), $skip_fields[$element_type]);
    return $fields;
  }

  function indexItem($index, $id, $item) {
    $element_type = ($index->item_type == 'relation') ? 'relation' : 'node';

    $element_index = $this->getDefaultIndex($element_type);
    $current_index = $this->connection->getIndex($index->name, $element_type);
    
    $uuid = $this->create_id($index->item_type, $id);
    $element = $this->getElement($element_type, $uuid); 

    //create Element
    if(!$element) {
      $element = $this->connection->createElement($element_type);
    }

    //update with new data
    $this->updateElement($element, $element_type, $uuid, $item);
    $fields = $this->getFields($element_type, $item);
    
    //index in both indexes
    $this->indexElement($element, $element_index, array('uuid'));
    $this->indexElement($element, $current_index, $fields);
  }
  
  function indexItems(SearchApiIndex $index, array $items) {
    foreach($items as $id => $item) {
      $this->indexItem($index, $id, $item);
    }
    
    return array();
  }
  
  function deleteItem($index, $id) {
    $element_type = ($index->item_type == 'relation') ? 'relation' : '';
    $element_index = $this->getDefaultIndex($element_type);
    
    $uuid = $this->create_id($index->item_type, $id);
    $element = $this->getElement($element_type, $uuid);
    
    if (!$element) {
      return;
    }
    
    //Remove from default index and current index
    $element_index->remove($element);
    
    $current_index = $this->connection->getIndex($index->name, $element_type);
    $current_index->remove($element);
    
    $element->delete();
  }
  
  function deleteItems($ids = 'all', \SearchApiIndex $index = NULL) {
    $db = debug_backtrace();
    dpm($db);

    if($ids == 'all') {
      //@TODO - Delete all
    }
    else {
      foreach ($ids as $id) {
        $this->deleteItem($index, $id);
      }
    }
  }
  
  function search(\SearchApiQueryInterface $query) {
    ;
  }
}