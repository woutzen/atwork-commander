#include <atwork_refbox_ros_task_generator/TaskGenerator.h>

#include <ros/console.h>

#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <regex>
#include <iterator>
#include <random>
#include <numeric>

using namespace std;


namespace atwork_refbox_ros {

class Object;
class Table;

using ObjectPtr = Object*;
using TablePtr = Table*;

enum class Orientation : unsigned int {
  FREE,
  VERTICAL,
  HORIZONTAL
};

enum class Type : unsigned int {
  UNKNOWN,
  OBJECT,
  COLORED_OBJECT,
  CAVITY,
  CONTAINER
};

struct ObjectBase {
  static Type extractType(const string& typeName) {
    size_t len = typeName.length();
    char preLast = typeName[ len - 2 ];
    if( preLast == '_' ) {
      char last = typeName[ len - 1 ];
      if (  last == 'H' || last == 'V' )
        return Type::CAVITY;
      if (  last == 'G' || last == 'B' )
        return Type::COLORED_OBJECT;
    }
    if ( regex_match( typeName, regex( "CONTAINER_.*" ) ) )
      return Type::CONTAINER;
    return Type::OBJECT;
  }

  string extractForm(const string& typeName) const {
    switch ( type ) {
      case ( Type::CAVITY):
      case ( Type::COLORED_OBJECT): return typeName.substr( 0, typeName.length() - 2 );
      case ( Type::CONTAINER ): return "DEFAULT";
      default: return typeName;
    }
  }

  string extractColor(const string& typeName) const {
    switch ( type ) {
      case ( Type::COLORED_OBJECT): return typeName.substr( typeName.length() - 1 );
      case ( Type::CONTAINER ): return typeName.substr( strlen("CONTAINER_") );
      default: return "DEFAULT";
    }
  }

  Orientation extractOrientation(const string& typeName) const {
    if ( type ==  Type::CAVITY)
      switch ( typeName[ typeName.length() - 1 ] ) {
        case ( 'H' ): return Orientation::HORIZONTAL;
        case ( 'V' ): return Orientation::VERTICAL;
      };
    return Orientation::FREE;
  }

  Type type = Type::UNKNOWN;
  string form = "";
  string color = "";
  Orientation orientation = Orientation::FREE;
  ObjectBase() = default;
  ObjectBase(const string typeName)
    : type( extractType( typeName ) ), form( extractForm( typeName ) ),
      color( extractColor( typeName ) ),
      orientation( extractOrientation( typeName ) )
  {}
};

struct ObjectType : public ObjectBase {
  unsigned int count = 0;
  ObjectType( const string& typeName, unsigned int count )
    : ObjectBase( typeName ), count(count)
  {}
  operator bool() const { return count; }
  ObjectType& operator--() {
    count--;
    return *this;
  }
  ObjectType operator--(int) {
    ObjectType temp(*this);
    count--;
    return temp;
  }
};

struct Object : public ObjectBase {
  static unsigned int globalID;
  unsigned int id=globalID++;
  TablePtr source = nullptr;
  TablePtr destination = nullptr;
  ObjectPtr container = nullptr;
  Object() = default;
  Object(const ObjectType& type): ObjectBase(type) {}
  Object(const Object& copy): ObjectBase(copy), id(copy.id) { }
  static void reset() { globalID = 0; }
};

unsigned int Object::globalID = 0;

struct Table {
  std::string name ="";
  std::string type ="";
  Table() = default;
  Table(const std::string& name, const std::string& type)
    : name(name), type(type) {}
};

}

ostream& operator<<(ostream& os, const atwork_refbox_ros::Type type) {
  switch ( type ) {
    case ( atwork_refbox_ros::Type::CAVITY ):         return os << "Cavity";
    case ( atwork_refbox_ros::Type::CONTAINER ):      return os << "Container";
    case ( atwork_refbox_ros::Type::COLORED_OBJECT ): return os << "Colored Object";
    case ( atwork_refbox_ros::Type::OBJECT ):         return os << "Plain Object";
    default:                                          return os << "UNKNOWN";
  }
}

ostream& operator<<(ostream& os, const atwork_refbox_ros::Orientation o) {
  switch ( o ) {
    case( atwork_refbox_ros::Orientation::VERTICAL )  : return os << "V";
    case( atwork_refbox_ros::Orientation::HORIZONTAL ): return os << "H";
    case( atwork_refbox_ros::Orientation::FREE )      : return os << "FREE";
    default                                           : return os << "UNKNOWN";
  }
}

ostream& operator<<(ostream& os, const atwork_refbox_ros::ObjectBase& type) {
  switch ( type.type ) {
    case ( atwork_refbox_ros::Type::CAVITY ):         return os << type.form << "_" << type.orientation;
    case ( atwork_refbox_ros::Type::CONTAINER ):      return os << "CONTAINER_"     << type.color;
    case ( atwork_refbox_ros::Type::COLORED_OBJECT ): return os << type.form << "_" << type.color;
    case ( atwork_refbox_ros::Type::OBJECT ):         return os << type.form;
    default:                                           return os << "UNKNOWN OBJECT TYPE";
  }
}

ostream& operator<<(ostream& os, const atwork_refbox_ros::Table& t) {
  return os << "Table " << t.name << "(" << t.type << "):";
}

ostream& operator<<(ostream& os, const atwork_refbox_ros::Object& o) {
  os << "Object " << atwork_refbox_ros::ObjectBase(o) << "(" << o.id << "):";
  if ( o.source )      os << " Src: " << o.source->name      << "(" << o.source->type      << ")";
  if ( o.destination ) os << " Dst: " << o.destination->name << "(" << o.destination->type << ")";
  if ( o.container )   os << " Cont: " << o.container->type   << "(" << o.container->id     << ")";
  return os;
}

ostream& operator<<(ostream& os, const vector<atwork_refbox_ros::Object>& v) {
  for (size_t i=0; i<v.size(); i++)
    os << v[i] << (i+1!=v.size()?"\n":"");
  return os;
}

template<typename T>
ostream& operator<<(ostream& os, const vector<T*>& v) {
  for (size_t i=0; i<v.size(); i++)
    os << *v[i] << (i+1!=v.size()?" ":"");
  return os;
}

template<typename T>
ostream& operator<<(ostream& os, const vector<T>& v) {
  for (size_t i=0; i<v.size(); i++)
    os << v[i] << (i+1!=v.size()?" ":"");
  return os;
}

template<typename K, typename V>
ostream& operator<<(ostream& os, const unordered_multimap<K, V>& m) {
  for (const auto& item: m)
    os << item.first << " = " << item.second << endl;
  return os;
}

namespace atwork_refbox_ros {

/** Task Generation Implementation
 *
 * Implements the generation of Task according to supplied configurations.
 *
 * 
 *
 **/
class TaskGeneratorImpl {

  static auto extractCavities(const ArenaDescription& arena) {
    vector<ObjectType> cavities;
      for (const auto& item: arena.cavities)
        if ( item.second )
          cavities.emplace_back(item.first, item.second);
    return cavities;
  }

  default_random_engine mRand;
  TaskDefinitions mTasks;
  unordered_multimap<string, Table> mTables;
  const vector<ObjectType> mAvailableCavities;

  auto extractObjectTypes(const string& task) {
    vector<ObjectType> availableObjects;
    for ( const auto& item: mTasks[task] )
      if ( item.second && regex_match( item.first, regex("[A-Z0-9_]+") ) )
        availableObjects.emplace_back(item.first, item.second);
    return availableObjects;
  }

  void sanityCheck(std::string task, const vector<ObjectType>& availableObjects) {
    if (mTasks[task]["object_count"]>=0 && availableObjects.empty() ) {
      ostringstream os;
      os << task << ": Transportation Task without allowed object defined!";
      throw runtime_error(os.str());
    }
    if (mTasks[task]["waypoint_count"]>=0 && mTables.size()<mTasks[task]["waypoint_count"] ) {
      ostringstream os;
      os << task << ": Navigation Task without enough workstations defined!";
      throw runtime_error(os.str());
    }
    if ( ( mTasks[task]["shelf_grasping"] || mTasks[task]["shelf_picking"] ) && mTables.count("SH")==0 ) {
      ostringstream os;
      os << task << ": Transportation Task involving shelf requested in Arena without it!";
      throw runtime_error(os.str());
    }
    if ( ( mTasks[task]["rt_grasping"] || mTasks[task]["rt_picking"] ) && mTables.count("TT")==0 ) {
      ostringstream os;
      os << task << ": Transportation Task involving Rotating Table requested in Arena without it!";
      throw runtime_error(os.str());
    }
    if ( mTasks[task]["pp"] && mTables.count("PP")==0 ) {
      ostringstream os;
      os << task << ": Transportation Task involving Precision Placement requested in Arena without it!";
      throw runtime_error(os.str());
    }
    if ( mTasks[task]["table_height_0"] && mTables.count("00")==0 ) {
      ostringstream os;
      os << task << ": Transportation Task involving zero height table requested in Arena without it!";
      throw runtime_error(os.str());
    }
    if ( mTasks[task]["table_height_5"] && mTables.count("05")==0 ) {
      ostringstream os;
      os << task << ": Transportation Task involving 5cm table requested in Arena without it!";
      throw runtime_error(os.str());
    }
    if ( mTasks[task]["table_height_10"] && mTables.count("10")==0 ) {
      ostringstream os;
      os << task << ": Transportation Task involving 5cm table requested in Arena without it!";
      throw runtime_error(os.str());
    }
    if ( mTasks[task]["table_height_15"] && mTables.count("15")==0 ) {
      ostringstream os;
      os << task << ": Transportation Task involving 5cm table requested in Arena without it!";
      throw runtime_error(os.str());
    }
    //TODO
  }

  void sanityCheck() {
    if (mTables.size() < 2) throw runtime_error("At least two tables need to exist in the arena!");
    if (mTasks.empty()) throw runtime_error("No Tasks configured!");
    for (auto& task : mTasks ) {
      if ( task.second["object_count"] == 0 && task.second["waypoint_count"] == 0 ) {
        ostringstream os;
        os << task.first << ": Empty Task defined!";
        throw runtime_error(os.str());
      }
    }
  }

  vector<TablePtr> extractTablesByTypes(vector<string> types) {
    vector<TablePtr> tables;
    size_t numTables = accumulate(types.begin(), types.end(), 0, [this](size_t n, const string& s){ return n + mTables.count( s ); } );
    tables.resize( numTables );
    auto start = tables.begin();
    for ( const string& type : types ) {
      auto its = mTables.equal_range( type );
      start = transform(its.first, its.second, start, []( decltype(mTables)::value_type& t ){ return &t.second; } );
    }
    return tables;
  }

  static vector<Object*> toPtr(vector<Object>::iterator start, vector<Object>::iterator end) {
    vector<Object*> ptrs( end - start );
    transform( start, end, ptrs.begin(), [](Object& t){ return &t; } );
    return ptrs;
  }

  vector<Object*> generateCavities( TaskDefinition& def, vector<ObjectType> availableCavities, vector<Object>& objects )
  {
    size_t cavitiesPerPPT = 5;
    size_t n = mTables.count( "PP" );
    size_t numCavitiesAvailable = accumulate(availableCavities.begin(), availableCavities.end(), 0ul, [](size_t n, ObjectType& t){ return t.count + n; });
    if ( n*cavitiesPerPPT > numCavitiesAvailable )
      ROS_WARN_STREAM_NAMED("generator", "[REFBOX] Not enough cavities available for " << n << " PP tables! Available: "
                            << numCavitiesAvailable << ", Needed: " << n*cavitiesPerPPT << "!");
    size_t cavitiesToGenerate = min(n*cavitiesPerPPT, numCavitiesAvailable);
    objects.resize( objects.size() + cavitiesToGenerate );
    auto start = objects.end() - cavitiesToGenerate;
    shuffle( availableCavities.begin(), availableCavities.end(), mRand );
    transform( availableCavities.begin(), availableCavities.begin() + cavitiesToGenerate, start,
               [](const ObjectType& t){ return Object(t); }
             );

    uniform_int_distribution<size_t> rand(0, n-1);
    auto ppTables = extractTablesByTypes( { "PP" } );
    auto cavities = toPtr(start, objects.end());

    ROS_DEBUG_STREAM_NAMED("generator", "[REFBOX] Generated Cavities:" << endl << cavities);
    for (auto& cavityPtr : cavities) {
      Table* table = ppTables[ rand(mRand) ];
      cavityPtr->source = table;
      cavityPtr->destination = table;
    }

    ROS_DEBUG_STREAM_NAMED("generator", "[REFBOX] Generated Objects (after Cavity Generation):" << endl << objects);
    return cavities;
  }

  vector<Object*> generateContainers( TaskDefinition& def,  vector<ObjectType>& availableObjects, vector<Object>& objects)
  {
    size_t numContainersAvailable = accumulate( availableObjects.begin(), availableObjects.end(), 0ul,
                                                [](size_t n, ObjectType& t){ return n + ( t.type == Type::CONTAINER ? t.count : 0 ); });
    objects.resize( objects.size() + numContainersAvailable );
    auto start = objects.end() - numContainersAvailable;

    vector<string> allowedTableTypes { "00", "05", "10", "15"}; // TODO add to configuration
    if ( def[ "container_in_shelf" ] )
      allowedTableTypes.push_back("Shelf");

    auto tables = extractTablesByTypes( allowedTableTypes );

    uniform_int_distribution<size_t> rand(0, tables.size()-1);

    for ( const auto& type : availableObjects )
      if ( type.type == Type::CONTAINER )
        for ( size_t i = 0; i < type.count; i++) {
          Object temp(type);
          Table* table = tables[ rand( mRand ) ];
          temp.source = table;
          temp.destination = table;
          *start++ = temp;
        }
    ROS_DEBUG_STREAM_NAMED("generator", "[REFBOX] Generated Objects (after Container Generation):" << endl << objects);
    return toPtr( objects.end() - numContainersAvailable, objects.end());
  }

  void place( TaskDefinition& def, vector<ObjectType>& availableObjects, vector<Object>& objects, string tableType )
  {
    // TODO
  }

  void place( TaskDefinition& def, vector<ObjectType>& availableObjects, vector<Object>& objects, Object& container )
  {
    ROS_DEBUG_STREAM_NAMED("generator", "[REFBOX] Generate Object to be placed in Container " << container);
    // TODO
    if ( container.type == Type::CAVITY && def[ "pp_team_orientation" ] )
      container.orientation = Orientation::FREE;

    // TODO
  }

  void pick( TaskDefinition& def, vector<ObjectType>& availableObjects, vector<Object>& objects, string tableType )
  {

    // TODO

  }

  Task generate( const string& taskName){
    TaskDefinition& def = mTasks[ taskName ];
    Object::reset();
    vector<Object> objects;

    auto availableObjects = extractObjectTypes( taskName );
    auto availableCavities = mAvailableCavities; // TODO: filter according to allowed Objects from Task
    ROS_DEBUG_STREAM_NAMED("generator", "[REFBOX-GEN] Tables:\n" << mTables);
    ROS_DEBUG_STREAM_NAMED("generator", "[REFBOX-GEN] Cavities:\n" << mAvailableCavities);
    ROS_DEBUG_STREAM_NAMED("generator", "[REFBOX-GEN] ObjectTypes:\n" << availableObjects);

    sanityCheck(taskName, availableObjects);
    auto seedIt = def.find("seed");
    if ( seedIt != def.end() ) mRand.seed(seedIt->second);


    // PP fill
    // generate places on PP
    // Container generate and distribute
    // generate places on Container
    // generate places on shelf
    // generate picks on shelf
    // generate picks on rt
    // generate remaining places
    // generate remaining picks


    auto cavities = generateCavities( def, availableCavities, objects);
    if ( cavities.size() < def[ "pp" ] ) {
      ostringstream os;
      os << "Not enough cavities generated! Generated " << cavities.size() << ", min Necessary: " << def[ "pp" ] << "!";
      throw runtime_error(os.str());
    }
    auto last = cavities.end()-1;
    for ( size_t i = 0; i < def[ "pp" ]; i++ ) {
      auto rand = uniform_int_distribution<size_t>( 0, last - cavities.begin()-1 );
      auto selected = cavities.begin() + rand( mRand );
      place( def, availableObjects, objects, **selected );
      iter_swap(selected, last);
      last--;
    }

    auto containers = generateContainers( def, availableObjects, objects );
    if ( containers.size() < 1 && def[ "container_placing" ] ) {
      ostringstream os;
      os << "Not enough containers generated! Generated " << containers.size() << ", min Necessary: 1!";
      throw runtime_error(os.str());
    }
    auto rand = uniform_int_distribution<size_t>( 0, containers.size()-1 );
    for ( size_t i = 0; i < def[ "container_placing" ]; i++) {
      place( def, availableObjects, objects, *containers[ rand(mRand) ] );
    }


    ROS_DEBUG_STREAM_NAMED("generator", "[REFBOX-GEN] Objects:\n" << objects);
    // TODO

    return Task();
  }

  public:
    TaskGeneratorImpl(const ArenaDescription& arena, const TaskDefinitions& tasks)
      : mTasks(tasks), mAvailableCavities( extractCavities( arena ) )
    {

      for (const auto& table: arena.workstations)
        mTables.emplace(table.second, Table(table.first, table.second));



      sanityCheck();
    }

    Task operator()(std::string taskName) {
      auto task = find_if(mTasks.begin(), mTasks.end(),
                         [taskName](const auto& item){ return item.first == taskName; }
                  );
      if (task == mTasks.end()) {
        ostringstream os;
        os << "No Task " << taskName << " configured. Valid tasks are: ";
        for (const auto& item: mTasks)
          os << item.first << " ";
        throw runtime_error(os.str());
      }
      return generate(task->first);
    }
};

TaskGenerator::TaskGenerator(const ArenaDescription& arena, const TaskDefinitions& tasks)
  : mImpl(new TaskGeneratorImpl(arena, tasks)) {}

TaskGenerator::~TaskGenerator() { delete mImpl; }

Task TaskGenerator::operator()(string taskName) { return mImpl->operator()(taskName); }

}
