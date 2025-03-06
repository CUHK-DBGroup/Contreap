//
//  db.h
//  YCSB-C
//
//  Created by Jinglei Ren on 12/18/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#ifndef YCSB_C_DB_FACTORY_H_
#define YCSB_C_DB_FACTORY_H_

#include <iostream>
#include <string>
#include <thread>

#include "core/db.h"
#include "core/properties.h"
#include "db/export.hpp"

namespace ycsbc {

class DBFactory {
public:
  static inline DB* CreateDB(utils::Properties &props) {
    try {
      if (props["dbname"] == "export") {
        return new Export(
          "export/" + props["filename"] + ".data",
          std::stoi(props.GetProperty(CoreWorkload::RECORD_COUNT_PROPERTY, "0")),
          std::stoi(props.GetProperty(CoreWorkload::OPERATION_COUNT_PROPERTY, "0")),
          props.GetProperty("updatedelete", "false") == "true"); } 
      else return NULL; }
    catch (const std::string &message) {
      std::cerr << message << std::endl;
      exit(0); }
  }

  static inline void DestroyDB(DB* db) {
    delete db;
  }
};

} // ycsbc

#endif // YCSB_C_DB_FACTORY_H_

