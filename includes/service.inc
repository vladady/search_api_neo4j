<?php

class SearchApiNeo4jService extends SearchApiAbstractService {

  protected $connection = NULL;
  private $node_index;
  private $relation_index;

  function __construct(SearchApiServer $server) {
    parent::__construct($server);

    $this->server = $server;
    $this->options = &$server->options;

    try {
      $this->connection = NeoFactory::getClient($server);

      // Init default indexes.
      $this->node_index = $this->connection->getIndex('node_index', 'node');
      $this->relation_index = $this->connection->getIndex('relation_index', 'relation');
    }
    catch (Exception $e) {
      watchdog('Search Api Neo4j', check_plain($e->getMessage()), array(), WATCHDOG_ERROR);
      drupal_set_message(check_plain($e->getMessage()), 'error');
      return FALSE;
    }

  }

  function configurationForm(array $form, array &$form_state) {
    if ($this->options) {
      $form['server_description'] = array(
        '#type' => 'item',
        '#title' => 'Neo4j server URI',
        '#description' => $this->options['host'].':'.$this->options['port'],
      );
    }

    $form['host'] = array(
      '#type' => 'textfield',
      '#title' => 'Host',
      '#default_value' => isset($this->options['host']) ? $this->options['host'] : 'localhost',
      '#required' => TRUE,
    );

    $form['port'] = array(
      '#type' => 'textfield',
      '#title' => 'Port',
      '#default_value' => isset($this->options['port']) ? $this->options['port'] : '7474',
      '#required' => TRUE,
    );

     $form['auth'] = array(
      '#type' => 'checkbox',
      '#title' => 'Needs Authentification',
      '#default_value' => isset($this->options['auth']) ? $this->options['auth'] : 1,
    );

    $form['user'] = array(
      '#type' => 'textfield',
      '#title' => 'Username',
      '#default_value' => isset($this->options['user']) ? $this->options['user'] : 'adm',
      '#states' => array(
        'invisible' => array(
          ':input[name="options[form][auth]"]' => array(
            'checked' => FALSE,
          ),
        ),
        'required' => array(
          ':input[name="options[form][auth]"]' => array(
            'checked' => TRUE,
          ),
        ),
      ),
    );

    $form['pass'] = array(
      '#type' => 'password',
      '#title' => 'Password',
      '#states' => array(
        'invisible' => array(
          ':input[name="options[form][auth]"]' => array(
            'checked' => FALSE,
          )
        ),
        'required' => array(
          ':input[name="options[form][auth]"]' => array(
            'checked' => TRUE,
          ),
        ),
      ),
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
    if (is_array($value)) {
      $type = substr($type, 5, -1);
    }

    // @TODO - make use of original type.
    switch ($type) {
      case 'duration':
      case 'integer':
      case 'date':
        return is_array($value) ? array_map('intval', $value) : intval($value);

      default :
        return $value;
    }
  }

  function getDefaultIndex($element_type) {
    if ($element_type == 'relation') {
      return $this->relation_index;
    }
    return $this->node_index;
  }

  function getElement($element_type, $uuid) {
    $element_index = $this->getDefaultIndex($element_type);
    return $element_index->findOne('uuid', $uuid);
  }

  function createEndpoint($endpoint) {
    $entity_id = $endpoint['entity_id'];
    $entity_type = $endpoint['entity_type'];

    $uuid = $this->create_id($entity_type, $entity_id);
    $node = $this->getElement('node', $uuid);
    if (!$node) {
      $node = $this->connection->createElement('node');
    }
    $this->updateElement($node, 'node', $uuid);

    // Add label.
    $labels = $this->connection->createLabels(array($entity_type));
    $node->addLabels($labels);

    $this->indexElement($node, $this->node_index, array('uuid'));

    return $node;
  }

  function updateElement($element, $element_type, $uuid, $item = array()) {
    if ($element_type == 'relation') {
      // Create relation.
      $relation_type = $item['relation_type']['value'];

      $source_node = $this->createEndpoint(reset($item['endpoints_list']['value']));
      $target_node = $this->createEndpoint(end($item['endpoints_list']['value']));

      // Update relation.
      $element->setStartNode($source_node)
          ->setEndNode($target_node)
          ->setType($relation_type);
    }

    $element->setProperty('uuid', $uuid);

    $fields = $this->getFields($element_type, $item);

    foreach ($fields as $field_name) {
      $field = $item[$field_name];

      if (!empty($field['value'])) {
        // @TODO - cast values.
        $value = $this::cast($field['value'], $field['type'], $field['original_type']);
        $element->setProperty($field_name, $value);
      }
    }

    $element->save();
  }

  function indexElement($element, $index, $fields = array()) {
    $index->remove($element);

    foreach ($fields as $field_name) {
      $index->add($element, $field_name, $element->getProperty($field_name));
    }
  }

  function getFields($element_type, $item) {
    $skip_fields = array(
      'relation' => array(
        'endpoints_list',
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
    $current_index = $this->connection->getIndex($index->machine_name, $element_type);

    $uuid = $this->create_id($index->item_type, $id);
    $element = $this->getElement($element_type, $uuid);

    // Create element.
    if (!$element) {
      $element = $this->connection->createElement($element_type);
    }

    // Update with new data.
    $this->updateElement($element, $element_type, $uuid, $item);
    $fields = $this->getFields($element_type, $item);

    if ($element_type == 'node') {
      $labels = $this->connection->createLabels(array($index->item_type));
      $element->addLabels($labels);
    }

    // Index in both indexes.
    $this->indexElement($element, $element_index, array('uuid'));
    $this->indexElement($element, $current_index, $fields);
  }

  function indexItems(SearchApiIndex $index, array $items) {
    // @TODO - index in batch.
    $ids = array();

    foreach ($items as $id => $item) {
      $this->indexItem($index, $id, $item);
      $ids[] = $id;
    }

    return $ids;
  }

  function deleteItem($index, $uuid) {
    $element_type = ($index->item_type == 'relation') ? 'relation' : 'node';
    $element_index = $this->getDefaultIndex($element_type);

    // $uuid = $this->create_id($index->item_type, $id);
    $element = $this->getElement($element_type, $uuid);

    if (!$element) {
      return;
    }

    // Remove from default index and current index.
    $element_index->remove($element);

    $current_index = $this->connection->getIndex($index->machine_name, $element_type);
    $current_index->remove($element);

    $element->delete();
  }

  function deleteItems($ids = 'all', \SearchApiIndex $index = NULL) {
    // @TODO - delete in batch.

    if ($ids == 'all') {
      // Remove all data from all indexes from this server.
      if (!$index) {
        search_api_neo4j_delete_all($this->server);
      }
      else {
        // Remove data from an index.
        $element_type = ($index->item_type == 'relation') ? 'relation' : 'node';
        $current_index = $this->connection->getIndex($index->machine_name, $element_type);

        try {
          foreach ($current_index->query('*:*') as $rec) {
            // If nodes have relations you cannot delete them.
            if ($element_type == 'node') {
              $relation = $rec->getFirstRelationship();
              if ($relation) {
                throw new Exception('Try deleting relation index before!');
              }
            }

            $this->deleteItem($index, $rec->getProperty('uuid'));
          }
        } catch (Exception $ex) {
          throw new Exception('Some problem appeared while deleting data: ' . $ex->getMessage());
        }
      }
    }
    else {
      foreach ($ids as $id) {
        $uuid = $this->create_id($index->item_type, $id);
        $this->deleteItem($index, $uuid);
      }
    }
  }

  function search(\SearchApiQueryInterface $query) {
    // @TODO - handle the search part.
  }

}
