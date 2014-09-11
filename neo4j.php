<?php

/* 
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

use Everyman\Neo4j\Client;
use Everyman\Neo4j\Query;
use Everyman\Neo4j\Cypher\Query as Cypher;
use Everyman\Neo4j\Gremlin\Query as Gremlin;

class Neo {
	private $client; 
  private static $indexes;
  
	function __construct(Client $client) {
		$this->client = $client;
    $this->index_factory = new IndexFactory($this->client);
	}
  
	function queryEngine($engine) {
		return new QueryFactory($engine, $this->client);
	}

  function getQueryEngine($engine) {
    $client = $this->client;
    
    $callback = function ($template, $vars = array()) use ($engine, $client) {
      $class = "Everyman\\Neo4j\\$engine\\Query";
      return new $class($client, $template, $vars);
    };
    
    return $callback;
  }
  
  function getIndex($name, $type = 'node') {
    $indexes = &self::$indexes;
    
    if(!isset($indexes[$name])) {
      $indexes[$name] = $this->index_factory->create($this->client, $name, $type);
      $indexes[$name]->save();
    }
    //@todo - possible leak
    return $indexes[$name];
  }
  
  function createNode() {
    return new \Everyman\Neo4j\Node($this->client);
  }
  function createElement($element_type) {
    if($element_type == 'node') {
      return $this->createNode();
    }
  }
  function getNode($id) {
    return $this->client->getNode($id);
  }
  
  function getNodeByProperty($property_name, $property_value) {
    $engine = $this->queryEngine('Cypher');
    $query = 'MATCH (n { ' . $property_name . ' : "' . $property_value . '" }) 
              RETURN n LIMIT 1';
    
    $result = $engine->query($query)
       ->getResultSet();

    $exists = $result->count();
    
    if(!$exists) {
      $node = $this->createNode();
    }
    else {
      $row = $result->current();
      $id = $row['n']->getId();
      $node = $this->getNode($id); 
    }
    
    return $node;
  }
 
}


class NeoFactory {
	private static $connections;
  
	static function getClient($config) {
    //Add default options
    $config->options += array(
      'host' => 'localhost',
      'port' => '7474',
    );
    
		$url = 'http://' . $config->options['host'] . ':' . $config->options['port'];
    
		if(!isset($connections[$url])) {
			//Set basic auth options
			if(isset($config->options['auth'])) {
				$transport = new Transport($config->options['host'], $config->options['port']);
				$transport->setAuth($config->options['user'], $config->options['pass']);

				$client = new Client($transport);
			}
			else {
				$client = new Client($config->options['host'], $config->options['port']);
			}

			$connections[$url] = new Neo($client);
		}
  
		return $connections[$url];
	}
}

class QueryFactory{
	private $engine;
	private $client;

	function __construct($engine, $client) {
		$this->engine = $engine;
		$this->client = $client;
	}

	function query($template, $vars = array()) {
    $engine = $this->engine;
    $class = "Everyman\\Neo4j\\$engine\\Query";
		return new $class($this->client, $template, $vars);
	}
}

class IndexFactory{
  private $types;
  
  public function __construct() {
    $this->types = array(
      'node' => 'NodeIndex',
      'relation' => 'RelationshipIndex',
      'fulltext' => 'NodeFulltextIndex'
    );
  }
  public function create($client, $name, $type) {
    if(isset($this->types[$type])) {
      $type = $this->types[$type];
      $class = "Everyman\\Neo4j\\Index\\$type";
      return new $class($client, $name);
    }
  }
}

class IndexItem {
  function __construct($index) {
    
  }
  function from() {
    
  }
  function index() {
    
  }
}